
#include <QTime>
#include <QImage>
#include <QDebug>


extern "C" {
#include <fitz.h>
#include <mupdf.h>
}

#define PROFILING(msg) qDebug() << (msg) << stopWatch.restart() << "ms";


int main()
{
	QTime stopWatch;
	int dpi;
	fz_glyph_cache *glyphcache;
	fz_device *dev;
	fz_error error;
	
	stopWatch.start();

	glyphcache = fz_new_glyph_cache();
	PROFILING("Initialization");


	pdf_xref *xref;
	error = pdf_open_xref(&xref, "pgfmanual.pdf", NULL);
	error = pdf_load_page_tree(xref);
	PROFILING("Load document");

	pdf_page *page;
	fz_display_list *list;
	list = fz_new_display_list();
	error = pdf_load_page(&page, xref, 0);
	dev = fz_new_list_device(list);
	error = pdf_run_page(xref, page, dev, fz_identity);
	fz_free_device(dev);
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
		stopWatch.restart();
		QImage tmp;

		{
			float zoom;
			fz_matrix ctm;
			fz_bbox bbox;
			fz_pixmap *pix;

			zoom = dpi / 72.;
			ctm = fz_translate(0, -page->mediabox.y1);
			ctm = fz_concat(ctm, fz_scale(zoom, -zoom));
			ctm = fz_concat(ctm, fz_rotate(page->rotate));
			fz_rect r = {0, 0, w * 72. / dpi, h * 72. / dpi};
			bbox = fz_round_rect(fz_transform_rect(ctm, r));
			
			pix = fz_new_pixmap_with_rect(fz_device_bgr, bbox);
			fz_clear_pixmap_with_color(pix, 255);

			dev = fz_new_draw_device(glyphcache, pix);
			fz_execute_display_list(list, dev, ctm, bbox);
			fz_free_device(dev);

			QImage buffer(pix->samples, pix->w, pix->h, QImage::Format_ARGB32);
			tmp = buffer.copy();
			
			fz_drop_pixmap(pix);
		}

		qDebug() << dpi << stopWatch.restart();
		tmp.save(QString("mupdf_partial_%1dpi.png").arg(dpi));
	}

	qDebug() << "";
	qDebug() << "Render full page";
	qDebug() << "[dpi] [ms]";

	for (dpi = 100; dpi <= 1200; dpi += 100) {
		stopWatch.restart();
		QImage tmp;

		{
			float zoom;
			fz_matrix ctm;
			fz_bbox bbox;
			fz_pixmap *pix;

			zoom = dpi / 72;
			ctm = fz_translate(0, -page->mediabox.y1);
			ctm = fz_concat(ctm, fz_scale(zoom, -zoom));
			ctm = fz_concat(ctm, fz_rotate(page->rotate));
			bbox = fz_round_rect(fz_transform_rect(ctm, page->mediabox));

			pix = fz_new_pixmap_with_rect(fz_device_bgr, bbox);
			fz_clear_pixmap_with_color(pix, 255);

			dev = fz_new_draw_device(glyphcache, pix);
			fz_execute_display_list(list, dev, ctm, bbox);
			fz_free_device(dev);

			QImage buffer(pix->samples, pix->w, pix->h, QImage::Format_ARGB32);
			tmp = buffer.copy();

			fz_drop_pixmap(pix);
		}

		qDebug() << dpi << stopWatch.restart();
	}

	return 0;
}

