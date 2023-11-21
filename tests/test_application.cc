#include "../chat/chat.h"

int main(int argc, char** argv) {
    chat::Application app;
    if(app.init(argc, argv)) {
        return app.run();
    }
    return 0;
}