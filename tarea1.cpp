#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <algorithm>

// Mutex para sincronizar la salida a la consola
std::mutex mtx;
std::condition_variable cv; // Condicion variable para notificar a las hebras
bool avanzar = false; // Bandera para indicar si un auto debe avanzar
int autoSeleccionado = -1; // ID del auto seleccionado para avanzar
bool carreraTerminada = false; // Bandera para indicar que la carrera ha terminado
bool choquesHabilitados = false; // Bandera para indicar si los choques est谩n habilitados

// Emojis para los autos y choques
const std::string EMOJI_AUTO = "馃殫";
const std::string EMOJI_CHOQUE = "馃敟馃殫馃敟";

// Funci贸n para generar colores infinitos
std::string generarColor(int id) {
    int colorCode = 31 + (id % 7); // Genera colores del 31 al 36, con un ciclo infinito
    return "\033[" + std::to_string(colorCode) + "m";
}

// Funci贸n para imprimir el estado actual de la carrera
void imprimirProgreso(const std::vector<int>& distancias, int M, int N, const std::vector<bool>& autosChocados) {
    std::cout << "\nEstado de la carrera:\n";
    for (size_t i = 0; i < distancias.size(); ++i) {
        std::string color = generarColor(i); // Generar color din谩mico
        std::cout << color << "Auto" << i << ": ";
        int progreso = static_cast<int>(50.0 * distancias[i] / M); // Escalar la distancia a una barra de 50 caracteres

        // Mostrar el trayecto recorrido por el auto
        for (int j = 0; j < progreso; ++j) {
            std::cout << "=";  
        }

        // Mostrar el emoji de choque solo si el auto ha chocado
        if (autosChocados[i]) {
            std::cout << EMOJI_CHOQUE;  // Reemplazar el auto por el emoji de choque
        } else {
            std::cout << EMOJI_AUTO;  // Mostrar el emoji del auto
        }

        std::cout << " (" << distancias[i] << " metros)" << "\033[0m" << std::endl; // Resetear color
    }
    std::cout << std::endl;
}

void carreraAuto(int id, int M, std::vector<int>& distancias, std::vector<int>& ordenLlegada, int& lugar, std::vector<bool>& autosChocados, std::mt19937& gen, std::uniform_int_distribution<>& distAvance, std::uniform_int_distribution<>& distChoque) {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return avanzar && autoSeleccionado == id || carreraTerminada; });

        if (carreraTerminada) break; // Si la carrera ha terminado, salir del bucle

        // Verificar si el auto ya choc贸
        if (autosChocados[id]) {
            avanzar = false;
            cv.notify_all();
            continue; // Saltar este auto si ha chocado
        }

        // Verificar si el auto choca (probabilidad del 1%), solo si los choques est谩n habilitados
        if (choquesHabilitados && distChoque(gen) == 1) {
            autosChocados[id] = true;  // Marcar el auto como chocado
            std::cout << generarColor(id) << "Auto" << id << " ha chocado! Estado: " << EMOJI_CHOQUE << "\033[0m" << std::endl;
            avanzar = false;
            cv.notify_all();
            continue;
        }

        int avance = distAvance(gen);
        distancias[id] = std::min(distancias[id] + avance, M);

        std::cout << generarColor(id) << "Auto" << id << " avanza " << avance << " metros (total: " << distancias[id] << " metros)" << "\033[0m" << std::endl;

        // Si el auto alcanza la meta y no ha sido registrado en el orden de llegada
        if (distancias[id] == M && std::find(ordenLlegada.begin(), ordenLlegada.end(), id) == ordenLlegada.end()) {
            ordenLlegada.push_back(id);
            std::cout << generarColor(id) << "Auto" << id << " ha terminado la carrera en el lugar " << ++lugar << "!" << "\033[0m" << std::endl;
        }

        avanzar = false;
        cv.notify_all();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << "Uso: " << argv[0] << " <distancia_total> <numero_autos> [<choques (opcional)>]" << std::endl;
        return 1;
    }

    int M = std::stoi(argv[1]); // Distancia total de la carrera
    int N = std::stoi(argv[2]); // N煤mero de autos

    // Si se ha pasado un tercer argumento y es "1", habilitar los choques
    if (argc == 4 && std::stoi(argv[3]) == 1) {
        choquesHabilitados = true;
    }

    std::vector<int> distancias(N, 0); // Vector para almacenar la distancia recorrida por cada auto
    std::vector<int> ordenLlegada; // Vector para almacenar el orden de llegada de los autos
    std::vector<bool> autosChocados(N, false); // Inicializar el vector de choques con "false"
    int lugar = 0; // Contador para el lugar de llegada

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distAvance(1, 10);
    std::uniform_int_distribution<> distAuto(0, N - 1);
    std::uniform_int_distribution<> distChoque(1, 100); // Distribuci贸n para el choque (1% de probabilidad)

    std::vector<std::thread> threads;
    for (int i = 0; i < N; ++i) {
        threads.emplace_back(carreraAuto, i, M, std::ref(distancias), std::ref(ordenLlegada), std::ref(lugar), std::ref(autosChocados), std::ref(gen), std::ref(distAvance), std::ref(distChoque));
    }

    while (ordenLlegada.size() + std::count(autosChocados.begin(), autosChocados.end(), true) < N) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            do {
                autoSeleccionado = distAuto(gen);
            } while (std::find(ordenLlegada.begin(), ordenLlegada.end(), autoSeleccionado) != ordenLlegada.end() || autosChocados[autoSeleccionado]);
            avanzar = true;
        }
        cv.notify_all();

        // Esperar a que el auto seleccionado avance
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return !avanzar; });
        }

        // Imprimir el estado actual de la carrera
        imprimirProgreso(distancias, M, N, autosChocados);

        // Esperar un intervalo de tiempo aleatorio antes de la siguiente iteraci贸n
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Notificar a las hebras que la carrera ha terminado
    {
        std::lock_guard<std::mutex> lock(mtx);
        carreraTerminada = true;
        cv.notify_all();
    }

    // Esperar a que todos las hebras terminen
    for (auto& t : threads) {
        t.join();
    }

    // Imprimir la tabla final con el orden de llegada
    std::cout << "\nLugar\tAuto" << std::endl;
    std::cout << "----------------" << std::endl;
    for (size_t i = 0; i < ordenLlegada.size(); ++i) {
        std::cout << i + 1 << "\tAuto" << ordenLlegada[i] << std::endl;
    }

    return 0;
}
