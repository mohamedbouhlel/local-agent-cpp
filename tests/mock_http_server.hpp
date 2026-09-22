#pragma once

// Serveur HTTP minimal sur loopback (127.0.0.1, port éphémère) utilisé pour
// exercer HttpClient et Agent::send sans LLM réel ni service distant.
//
// Le serveur répond aux requêtes POST reçues en consommant une file de
// réponses scriptées, et conserve les corps des requêtes *complètes* reçues
// pour permettre les assertions côté test.
//
// Résilience : chaque connexion acceptée est lue avec un délai (SO_RCVTIMEO).
// Un client silencieux ou une requête incomplète ne bloque donc pas le serveur
// au-delà de ce délai : il reçoit une réponse 408 et la connexion est fermée,
// sans être enregistrée dans received_bodies(). Le destructeur réveille
// également la lecture en cours pour un arrêt prompt.

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class MockHttpServer {
public:
    struct Response {
        int status = 200;
        std::string body;
        // Si piece_size > 0, le corps est écrit par petits morceaux avec un
        // délai : provoque plusieurs callbacks d'écriture côté client.
        std::size_t piece_size = 0;
        int piece_delay_us = 0;
        // Déclare un Content-Length plus grand que le corps réellement envoyé
        // puis ferme : le client voit une réponse tronquée.
        bool truncated = false;
        // Ferme la connexion sans répondre du tout.
        bool abort = false;
    };

    // read_timeout_ms : délai maximal de lecture d'une connexion avant
    // abandon (408). Une valeur <= 0 retombe sur le défaut.
    explicit MockHttpServer(int read_timeout_ms = 2000);
    ~MockHttpServer();

    MockHttpServer(const MockHttpServer&) = delete;
    MockHttpServer& operator=(const MockHttpServer&) = delete;

    // URL complète pour l'endpoint demandé, ex. url("/v1/chat/completions").
    std::string url(const std::string& path) const;

    int port() const { return port_; }

    void enqueue(Response response);

    // Corps des requêtes reçues, dans l'ordre d'arrivée.
    std::vector<std::string> received_bodies() const;

    // Nombre de réponses scriptées encore en file (diagnostic).
    std::size_t pending_responses() const;

private:
    void run();
    void handle_connection(int connection);

    int listen_fd_ = -1;
    int port_ = 0;
    int read_timeout_ms_ = 2000;
    std::atomic<bool> stopped_{false};
    // Connexion en cours de traitement (-1 si aucune) : permet au destructeur
    // de reveiller une lecture bloquee au lieu d'attendre le delai complet.
    std::atomic<int> active_connection_{-1};
    std::thread thread_;

    mutable std::mutex mutex_;
    std::vector<Response> responses_;
    std::vector<std::string> bodies_;
};
