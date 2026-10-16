#include <string>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "window.hpp"
#include "network.hpp"
#include "options.hpp"

#include "models/video.hpp"

int main(int argc, char *argv[]) {
    QApplication javQt(argc, argv);

    /* Load options */
    Options options;

    /* Test reply */
    NetworkManager manager(options);
    //manager.get(QNetworkRequest(QUrl("http://qt-project.org")));
    manager.fetchSite(QUrl("https://www.javlibrary.com/en/javme33rtu.html"));

    /* Test window */
    MainWindow window;
    window.resize(1600, 900);
    window.setWindowTitle("JAV-Qt");
    window.show();

    return javQt.exec();
}
