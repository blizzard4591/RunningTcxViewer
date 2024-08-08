#include <iostream>

#include <QApplication>

#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
	std::cout << "TcxViewer" << std::endl;

	QApplication app(argc, argv);

	MainWindow w;
	w.show();
	return app.exec();
}

#ifdef _MSC_VER
int __stdcall WinMain(struct HINSTANCE__*, struct HINSTANCE__*, char*, int) {
	return main(__argc, __argv);
}

#endif
