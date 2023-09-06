#include <bisemutum/engine/engine.hpp>

int main(int argc, char **argv) {
    if (!bi::initialize_engine(argc, argv)) { return -1; }

    bi::g_engine->execute();

    if (!bi::finalize_engine()) { return -2; }
    return 0;
}
