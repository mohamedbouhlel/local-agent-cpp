#!/usr/bin/env python3
"""Shared PTY experiment. Handler ACKs and PTY bytes are deliberately separate."""
import argparse
import fcntl
import os
import pty
import select
import struct
import subprocess
import termios
import time

KEYS=[b"\x1bOP",b"\x1bOQ",b"\x1bOR",b"\x1bOS",b"\x1b[15~",b"\x1b[17~"]
SIZES=[(4,10),(12,40),(6,20),(24,80)]*4

def run(exe, mode="normal"):
    master,slave=pty.openpty();fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack("HHHH",24,80,0,0))
    before=termios.tcgetattr(slave);readfd,writefd=os.pipe();os.set_inheritable(writefd,True);os.set_blocking(readfd,False)
    args=[exe,f"--protocol-fd={writefd}"]
    if mode=="exception":args.append("--throw-on-f12")
    if mode=="overload":args.append("--consumer-pause-ms=2000")
    if mode=="idle":args.extend(("--no-stream","--idle-exit-ms=350"))
    pid=os.fork()
    if pid==0:
        os.close(master);os.close(readfd);os.setsid();fcntl.ioctl(slave,termios.TIOCSCTTY,0);os.dup2(slave,0);os.dup2(slave,1);os.dup2(slave,2)
        os.execve(exe,args,dict(os.environ,TERM=("dumb" if mode=="term-dumb" else "xterm-256color"),LC_ALL="C.UTF-8"))
    os.close(writefd);start=time.monotonic();record_buf=bytearray();records=[];output=bytearray();pending=[];nav_ms=[];ack_times=[];samples=0
    cpu0=None;cpu_last=None;rss_max=0;next_cpu=0.0;resize_i=0;nav_i=0;post_nav=False;submitted=False;exit_sent=False;status=None;done=False
    timeout=14 if mode in ("normal","overload") else 3
    ncurses="ncursesw" in os.path.basename(exe)
    return_i=0;composer_returned=False
    while time.monotonic()-start<timeout:
        ready,_,_=select.select([master,readfd],[],[],0.005)
        new_lines=[]
        for fd in ready:
            try:data=os.read(fd,65536)
            except OSError:data=b""
            if fd==master:output.extend(data)
            else:
                record_buf.extend(data)
                while b"\n" in record_buf:
                    raw,_,rest=record_buf.partition(b"\n");record_buf=bytearray(rest);new_lines.append(raw.decode("utf-8","replace"))
        elapsed=time.monotonic()-start
        for line in new_lines:
            records.append(line)
            if line.startswith("ACK "):ack_times.append((int(line.split()[1]),elapsed))
            if line.startswith("PANEL ") and pending:nav_ms.append((elapsed-pending.pop(0))*1000)
        if elapsed>=next_cpu:
            next_cpu=elapsed+0.05
            try:
                stat=open(f"/proc/{pid}/stat").read().split();ticks=(int(stat[13])+int(stat[14]))/os.sysconf("SC_CLK_TCK")
                if cpu0 is None:cpu0=(elapsed,ticks)
                cpu_last=(elapsed,ticks)
                status_text=open(f"/proc/{pid}/status").read();rss_max=max(rss_max,int(next(x.split()[1] for x in status_text.splitlines() if x.startswith("VmRSS:"))))
            except (OSError,StopIteration,ValueError):pass
        if mode=="normal" and nav_i<len(KEYS)*40 and elapsed>=0.2+nav_i*0.015:
            pending.append(time.monotonic()-start);os.write(master,KEYS[nav_i%len(KEYS)]);nav_i+=1
        if mode=="normal" and nav_i==len(KEYS)*40 and not post_nav and elapsed>4.4:
            # Focus navigation, return to Conversation, and detach from the live tail.
            os.write(master,b"\t\x1bOP\x1b[5~"+(b"" if ncurses else b"\x1b[6~\t"));post_nav=True
        if mode=="normal" and ncurses and post_nav and return_i<100 and elapsed>=6.0+return_i*0.025:
            os.write(master,b"\x1b[6~");return_i+=1
        if mode=="normal" and ncurses and post_nav and return_i==100 and not composer_returned:
            os.write(master,b"\t");composer_returned=True
        if mode=="normal" and post_nav and not submitted and elapsed>(8.8 if ncurses else 4.8):
            # Insert in the middle, then preserve blank line, spaces and UTF-8.
            os.write(master,b"ab\x1bODX\x1bOC\n\n  caf\xc3\xa9\x1bOA\x1bOB\x1b[1;2F\x1bOD\x1bOC\x1b[20~");submitted=True
        if mode=="normal" and resize_i<len(SIZES) and elapsed>=0.7+resize_i*0.2:
            rows,cols=SIZES[resize_i];fcntl.ioctl(slave,termios.TIOCSWINSZ,struct.pack("HHHH",rows,cols,0,0));resize_i+=1
        if mode=="normal" and submitted and not exit_sent and elapsed>10.5:os.write(master,b"\x1b[21~");exit_sent=True
        if mode=="active-close" and not exit_sent and elapsed>0.7:os.write(master,b"\x1b[21~");exit_sent=True
        if mode=="exception" and not exit_sent and elapsed>0.2:os.write(master,b"\x1b[24~");exit_sent=True
        if mode=="overload" and not exit_sent and elapsed>12.5:os.write(master,b"\x1b[21~");exit_sent=True
        done,status=os.waitpid(pid,os.WNOHANG)
        if done:break
    if not done:os.kill(pid,9);_,status=os.waitpid(pid,0)
    drain_until=time.monotonic()+0.25
    while time.monotonic()<drain_until:
        ready,_,_=select.select([master,readfd],[],[],0.02)
        if not ready:continue
        for fd in ready:
            try:data=os.read(fd,65536)
            except OSError:data=b""
            if fd==master:output.extend(data)
            elif data:
                record_buf.extend(data)
                while b"\n" in record_buf:
                    raw,_,rest=record_buf.partition(b"\n");record_buf=bytearray(rest);records.append(raw.decode("utf-8","replace"))
    after=termios.tcgetattr(slave);restored=before==after;end=time.monotonic();os.close(slave);os.close(master);os.close(readfd)
    seq=[int(x.split()[1]) for x in records if x.startswith("ACK ")]
    submits=[x for x in records if x.startswith("SUBMIT_HEX ")]
    stream_ms=[(ack_times[i][1]-ack_times[i-1][1])*1000 for i in range(1,len(ack_times)) if ack_times[i][0]==ack_times[i-1][0]+1]
    stream_rate=round(999/(ack_times[-1][1]-ack_times[0][1]),1) if len(ack_times)>1 and ack_times[-1][0]==999 and ack_times[0][0]==0 else None
    p95=lambda xs:round(sorted(xs)[int((len(xs)-1)*0.95)],2) if xs else None
    cpu_pct=round((cpu_last[1]-cpu0[1])/(cpu_last[0]-cpu0[0])*100,1) if cpu0 and cpu_last and cpu_last[0]>cpu0[0] else None
    frame_line=next((x for x in records if x.startswith("FRAMES ")),"FRAMES unavailable")
    alt_on=b"\x1b[?1049h" in output;alt_off=b"\x1b[?1049l" in output
    print(f"{os.path.basename(exe)} {mode}: rc={os.waitstatus_to_exitcode(status)} ACK={len(seq)} ordered={seq==sorted(seq)} nav_n={len(nav_ms)} nav_p95_handler_ms={p95(nav_ms)} stream_rate_ACKs_s={stream_rate} stream_ack_interval_p95_ms={p95(stream_ms)} resize_n={sum(x.startswith('RESIZE ') for x in records)} {frame_line} RSS_peak_sampled_KiB={rss_max} CPU_sampled_pct={cpu_pct} PTY_bytes={len(output)} alt_screen_on={alt_on} alt_screen_off={alt_off} termios_equal={restored} overflow={[x for x in records if x.startswith('OVERFLOW ')][:1]} turn={[x for x in records if x.startswith('TURN_FINISHED ')][:1]}")
    assert restored,(mode,"termios changed") if mode in ("normal","exception","overload","active-close","idle") else True
    if mode=="normal":
        expected="aXb\n\n  café".encode().hex()
        assert submits and submits[0].split()[1]==expected,(submits,expected)
        assert seq==list(range(1000)),(len(seq),seq[:3],seq[-3:])
        assert len(nav_ms)==len(KEYS)*40,len(nav_ms)
        assert any(x.startswith("SCROLL ") for x in records)
        assert any(x=="FOCUS navigation" for x in records) and any(x=="FOCUS composer" for x in records)
        assert sum(x.startswith("RESIZE ") for x in records)>=12
        assert "LAYOUT tiny 10x4" in records
        assert any(x=="RESIZE 10x4" for x in records) and any(x=="RESIZE 40x12" for x in records)
        assert "TURN_FINISHED 1000" in records
        assert alt_on and alt_off
        safe=b"".join(bytes.fromhex(x.split()[1]) for x in records if x.startswith("SAFE_HEX "))
        assert safe and b"\x1b" not in safe and b"\x01" not in safe and b"\x7f" not in safe
        assert b"\xc2\x85" not in safe and b"\xe2\x80\xae" not in safe and b"\xf0\x28\x8c\x28" not in safe
        assert b"\xef\xbf\xbd" in safe and "café".encode() in safe and "RESIZE 10x4" in records and "RESIZE 40x12" in records
        if ncurses:
            up_i=next(i for i,line in enumerate(records) if line=="SCROLL 8")
            down_i=next(i for i,line in enumerate(records[up_i+1:],up_i+1) if line.startswith("SCROLL "))
            anchor=[]
            for line in records[up_i+1:down_i]:
                if line.startswith("VIEW_RANGE "):anchor.append(tuple(map(int,line.split()[1].split(":"))))
            assert len(anchor)>=2 and all(item[:2]==anchor[0][:2] for item in anchor),("viewport moved while reading history",anchor[:1],anchor[-1:])
            before_scroll=[tuple(map(int,line.split()[1].split(":"))) for line in records[:up_i] if line.startswith("VIEW_RANGE ")]
            assert before_scroll and before_scroll[-1][1]==before_scroll[-1][2],("initial bottom follow missing",before_scroll[-1:] )
            zero_i=max(i for i,line in enumerate(records) if line=="SCROLL 0")
            resumed=[tuple(map(int,line.split()[1].split(":"))) for line in records[zero_i+1:] if line.startswith("VIEW_RANGE ")]
            assert len(resumed)>=2 and all(end_i==total for _,end_i,total in resumed),("bottom follow not resumed",resumed[-3:])
    if mode=="exception":assert os.waitstatus_to_exitcode(status)==2 and alt_on and alt_off
    if mode=="overload":assert os.waitstatus_to_exitcode(status)==0 and any(x.startswith("OVERFLOW ") for x in records) and "TURN_FINISHED 1000" in records
    if mode=="active-close":assert os.waitstatus_to_exitcode(status)==0 and end-start<3
    if mode=="term-dumb":assert os.waitstatus_to_exitcode(status)==2 and b"incompatible" in output and b"\x1b" not in output
    if mode=="idle":assert os.waitstatus_to_exitcode(status)==0 and "FRAMES 1" in records,(records,)

def main():
    p=argparse.ArgumentParser();p.add_argument("ftxui");p.add_argument("ncursesw");p.add_argument("ftxui_sanitizer");p.add_argument("ncursesw_sanitizer");a=p.parse_args()
    for exe,test in ((a.ftxui,a.ftxui_sanitizer),(a.ncursesw,a.ncursesw_sanitizer)):
        r=subprocess.run([test],capture_output=True,text=True);print(r.stdout.strip());assert r.returncode==0,(test,r.stderr)
        for mode in ("normal","exception","overload","active-close","term-dumb"):run(exe,mode)
        pipe=subprocess.run([exe],input=b"",stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=dict(os.environ,TERM="xterm-256color"))
        assert pipe.returncode==2 and b"TTY requis" in pipe.stderr and b"\x1b" not in pipe.stdout
        print(f"{os.path.basename(exe)} non-TTY: refused; ANSI stdout absent")
    run(a.ncursesw,"idle")
if __name__=="__main__":main()
