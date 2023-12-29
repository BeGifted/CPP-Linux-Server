#include "chat/http2/http2_server.h"

void run() {
    chat::http2::Http2Server::ptr server(new chat::http2::Http2Server);
    chat::Address::ptr addr = chat::Address::LookupAnyIPAddress("0.0.0.0:8099");
    //server->bind(addr, true);
    //server->loadCertificates("rootCA.pem", "rootCA.key");

    server->bind(addr, false);
    server->start();
}

int main(int argc, char** argv) {
    chat::IOManager iom(2);
    iom.schedule(run);
    iom.stop();
    return 0;
}