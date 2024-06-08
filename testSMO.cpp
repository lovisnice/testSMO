#include <iostream>
#include <queue>
#include <random>
#include <atomic>
#include <vector>

// Global sequence counter
std::atomic<int> sequence_counter(0);

// Request class definition
class Request {
public:
    Request(int seq, int prio, int pha) : sequence_number(seq), priority(prio), phase(pha) {}

    int get_sequence_number() const { return sequence_number; }
    int get_priority() const { return priority; }
    int get_phase() const { return phase; }

private:
    int sequence_number;
    int priority;
    int phase;
};

// Stream class definition
class Stream {
public:
    Stream() : processed_requests(0), lost_requests(0) {}

    void add_request(const Request& request) {
        requestsStream.push(request);
    }

    // Function to retrieve requests by phase and priority
    std::vector<Request> retrieve_requests(int phase, int min_priority) {
        std::queue<Request> tempQueue;
        std::vector<Request> retrievedRequests;

        while (!requestsStream.empty()) {
            Request req = requestsStream.front();
            requestsStream.pop();
            if (req.get_priority() != 0 && req.get_phase() == phase) {
                retrievedRequests.push_back(req);
            }
            else {
                tempQueue.push(req);
            }
        }

        // Restore the remaining requests back to the original queue
        requestsStream = tempQueue;

        return retrievedRequests;
    }

    // Function to display the queue for debugging purposes
    void display_requests() const {
        std::queue<Request> q = requestsStream; // Make a copy to display contents
        std::cout << "Requests in Stream: ";
        while (!q.empty()) {
            const Request& req = q.front();
            std::cout << "(" << req.get_sequence_number() << ", " << req.get_priority() << ", " << req.get_phase() << ") ";
            q.pop();
        }
        std::cout << std::endl;
    }

    // Function to clear the remaining requests and mark them as lost
    void clear_requests() {
        std::cout << "Lost Requests: ";
        while (!requestsStream.empty()) {
            const Request& req = requestsStream.front();
            std::cout << "(" << req.get_sequence_number() << ", " << req.get_priority() << ", " << req.get_phase() << ") ";
            requestsStream.pop();
            set_lost_requests();
        }
        std::cout << std::endl;
    }

    int get_processed_requests() const { return processed_requests; }
    int get_lost_requests() const { return lost_requests; }
    void set_processed_requests() { this->processed_requests++; }
    void set_lost_requests() { this->lost_requests++; }

private:
    std::queue<Request> requestsStream;
    int processed_requests;
    int lost_requests;
};

// Generator class definition
class Generator {
public:
    Generator(Stream& stream)
        : mt(rd()), dist(0.0, 1.0), stream(stream) {}

    void generate_request() {
        int priority = 0;
        float probability = dist(mt);
        if (probability > 0.3) {
            priority = 1;
        }
        Request request(++sequence_counter, priority, 1); // Assume phase is always 1 for now
        stream.add_request(request);
    }

private:
    std::random_device rd;
    std::mt19937 mt;
    std::uniform_real_distribution<float> dist;
    Stream& stream;
};

// Queue class definition
class Queue {
public:
    Queue(Stream& stream, int max_size) : stream(stream), max_size(max_size) {}

    // Function to add a request to the queue
    bool enqueue(const Request& request) {
        if (queue.size() < max_size) {
            queue.push(request);
            return true;
        }
        else {
            return false;
        }
    }

    // Function to check if the queue is full
    bool is_full() const {
        return queue.size() >= max_size;
    }

    // Function to check if the queue is empty
    bool empty() const {
        return queue.empty();
    }

    // Function to get the front request in the queue
    const Request& front() const {
        if (!queue.empty()) {
            return queue.front();
        }
        else {
            throw std::out_of_range("Queue is empty");
        }
    }

    // Function to remove the front request from the queue
    void pop() {
        if (!queue.empty()) {
            queue.pop();
        }
        else {
            throw std::out_of_range("Queue is empty");
        }
    }

    // Function to display the current state of the queue
    void display_queue() const {
        std::queue<Request> q = queue; // Make a copy to display contents
        while (!q.empty()) {
            const Request& req = q.front();
            std::cout << "(" << req.get_sequence_number() << ", " << req.get_priority() << ", " << req.get_phase() << ") ";
            q.pop();
        }
        std::cout << std::endl;
    }

    // Function to clear the queue and mark remaining requests as lost
    void clear_queue() {
        std::cout << "Lost Requests in Queue: ";
        while (!queue.empty()) {
            const Request& req = queue.front();
            std::cout << "(" << req.get_sequence_number() << ", " << req.get_priority() << ", " << req.get_phase() << ") ";
            queue.pop();
            stream.set_lost_requests();
        }
        std::cout << std::endl;
    }

private:
    std::queue<Request> queue;
    Stream& stream; // Reference to the stream
    int max_size;   // Maximum number of requests the queue can hold
};

// Channel class definition
class Channel {
public:
    Channel(Stream& stream) : stream(stream) {}

    // Function to process a request from the queue
    void process_request(Queue& queue) {
        if (!queue.empty()) {
            const auto& req = queue.front();
            std::cout << "(" << req.get_sequence_number() << ", " << req.get_priority() << ", " << req.get_phase() << ") ";
            queue.pop();
            stream.set_processed_requests();
        }
    }

private:
    Stream& stream;
};

// Phase class definition
class Phase {
public:
    Phase(Stream& stream, int phase_number, int num_queues, int max_requests_per_queue, int num_channels)
        : stream(stream), phase_number(phase_number), max_requests_per_queue(max_requests_per_queue), num_channels(num_channels)
    {
        for (int i = 0; i < num_queues; ++i) {
            queues.emplace_back(stream, max_requests_per_queue);
        }
        for (int i = 0; i < num_channels; ++i) {
            channels.emplace_back(stream);
        }
    }

    // Function to distribute requests to queues based on phase
    void distribute_to_queues() {
        auto requests_phase = stream.retrieve_requests(phase_number, 1);
        for (const auto& req : requests_phase) {
            bool enqueued = false;
            for (auto& queue : queues) {
                if (!queue.is_full()) {
                    queue.enqueue(req);
                    enqueued = true;
                    break;
                }
            }
            if (!enqueued) {
                std::cout << "All queues are full. Cannot enqueue more requests for phase " << phase_number << std::endl;
                stream.set_lost_requests(); // Marking the request as lost
            }
        }
    }

    // Function to process a maximum of `num_channels` requests from all queues
    void process_requests_in_phase() {
        int total_processed = 0;

        for (int i = queues.size() - 1; i >= 0; --i) {
            auto& queue = queues[i];
            if (!queue.empty() && total_processed < num_channels) {
                for (auto& channel : channels) {
                    if (!queue.empty() && total_processed < num_channels) {
                        std::cout << "Processed request from Queue using Channel: ";
                        channel.process_request(queue);
                        total_processed++;
                        std::cout << std::endl;
                    }
                }
            }
        }
    }

    void clear_queues_in_phase()
    {
        for (auto& queue : queues) {
            queue.clear_queue();

        }
    }

    // Function to display the current state of all queues in the phase
    void display_all_queues() const {
        for (size_t i = 0; i < queues.size(); ++i) {
            std::cout << "Queue " << i + 1 << ": ";
            queues[i].display_queue();
        }
    }

private:
    Stream& stream;
    int phase_number;
    int max_requests_per_queue;
    int num_channels;
    std::vector<Queue> queues; // Vector of queues for this phase
    std::vector<Channel> channels; // Vector of channels for this phase
};

int main() {
    Stream stream;  // Create a stream
    Generator generator1(stream);
    Generator generator2(stream);
    Generator generator3(stream);

    Phase phase(stream, 1, 3, 4, 2);
    for (int i = 0; i < 20; ++i) {
        // Generate several requests
        generator1.generate_request();
        generator2.generate_request();
        generator3.generate_request();

        // Display the contents of the stream
        stream.display_requests();

        // Create a Phase object with 3 queues, each with a maximum of 4 requests, and 2 channels

        phase.distribute_to_queues();
        phase.display_all_queues(); // Display queues after distribution

        // Process a maximum of `num_channels` requests in this phase
        phase.process_requests_in_phase();
        phase.display_all_queues(); // Display queues after processing

        // Clear remaining requests and mark them as lost
        std::cout << "_____________________________" << std::endl;
    }

    // Clear remaining requests and mark them as lost for all queues
    phase.clear_queues_in_phase();
    stream.clear_requests();
    std::cout << "Lost requests: " << stream.get_lost_requests() << std::endl;
    std::cout << "Processed requests: " << stream.get_processed_requests() << std::endl;

    return 0;
}

