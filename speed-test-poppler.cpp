
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
	doc->setRenderBackend(Poppler::Document::SplashBackend);
	doc->setRenderHint(Poppler::Document::Antialiasing);
	doc->setRenderHint(Poppler::Document::TextAntialiasing);
	PROFILING("Load document");

	Poppler::Page * page = doc->page(0);
	PROFILING("Load page");

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
		
		tmp.save(QString("poppler_partial_%1dpi.png").arg(dpi));
	}

	qDebug() << "";
	qDebug() << "Render full page";
	qDebug() << "[dpi] [ms]";

	for (dpi = 100; dpi <= 1200; dpi += 100) {
		stopWatch.restart();
		QImage tmp;
		tmp = page->renderToImage(dpi, dpi);
		qDebug() << dpi << stopWatch.restart();
	}

	return 0;
}

