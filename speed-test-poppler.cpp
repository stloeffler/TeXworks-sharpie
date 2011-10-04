
#include <QTime>
#include <QImage>
#include <poppler/qt4/poppler-qt4.h>
#include <QDebug>

#define PROFILING(msg) qDebug() << (msg) << stopWatch.restart() << "ms";


int main()
{
	QTime stopWatch;
	int dpi;
	
	stopWatch.start();

	Poppler::Document * doc = Poppler::Document::load("pgfmanual.pdf");
	PROFILING("Load document");

	Poppler::Page * page = doc->page(0);
	PROFILING("Load page");

	qDebug() << "";
	qDebug() << "Render full page";
	qDebug() << "[dpi] [ms]";

	for (dpi = 100; dpi <= 1200; dpi += 100) {
		stopWatch.restart();
		QImage tmp;
		tmp = page->renderToImage(dpi, dpi);
		qDebug() << dpi << stopWatch.restart();
	}

	int w, h;
	int x, y;
//	w = page->pageSize().width() * 100 / 72;
//	h = page->pageSize().height() * 100  / 72;
	w = h = 800;

	qDebug() << "";
	qDebug() << "Render" << w << "x" << h << "px portion";
	qDebug() << "[dpi] [ms]";

	for (dpi = 100; dpi <= 1200; dpi += 100) {
		x = page->pageSize().width() * dpi / 72 - w;
		y = page->pageSize().height() * dpi / 72 - h;
		stopWatch.restart();
		QImage tmp;
		tmp = page->renderToImage(dpi, dpi, x, y, w, h);
		qDebug() << dpi << stopWatch.restart();
	}

	return 0;
}

