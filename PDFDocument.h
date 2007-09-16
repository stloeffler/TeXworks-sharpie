#ifndef PDFDocument_H
#define PDFDocument_H

#include <QMainWindow>
#include <QImage>
#include <QLabel>

#include "poppler-qt4.h"

#include "ui_PDFDocument.h"

class QAction;
class QMenu;
class QToolBar;
class QScrollArea;
class TeXDocument;

class PDFMagnifier : public QLabel
{
	Q_OBJECT

public:
	PDFMagnifier(QWidget *parent);
	void setPage(Poppler::Page *p, double scale);

protected:
	virtual void paintEvent(QPaintEvent *event);

private:
	Poppler::Page	*page;
	double	scaleFactor;

	QImage	image;
};

class PDFWidget : public QLabel
{
	Q_OBJECT

public:
	PDFWidget();
	void setDocument(Poppler::Document *doc);

private slots:
	void goFirst();
	void goPrev();
	void goNext();
	void goLast();
	void doPageDialog();
	
	void actualSize();
	void fitWidth();
	void fitWindow();
	void zoomIn();
	void zoomOut();

public slots:
	void windowResized();

protected:
	virtual void paintEvent(QPaintEvent *event);

	virtual void mousePressEvent(QMouseEvent *event);
	virtual void mouseReleaseEvent(QMouseEvent *event);
	virtual void mouseMoveEvent(QMouseEvent *event);

private:
	void init();
	void reloadPage();
	void adjustSize();
	void updateStatusBar();
	
	Poppler::Document	*document;
	Poppler::Page		*page;

	int pageIndex;
	double	scaleFactor;
	typedef enum {
		kFixedMag,
		kFitWidth,
		kFitWindow
	} autoScaleOption;
	autoScaleOption scaleOption;
	
	QImage	image;

	PDFMagnifier	*magnifier;
};


class PDFDocument : public QMainWindow, private Ui::PDFDocument
{
	Q_OBJECT

public:
	PDFDocument(const QString &fileName, TeXDocument *sourceDoc);

	static PDFDocument *findDocument(const QString &fileName);

protected:
//	void closeEvent(QCloseEvent *event);
	virtual void resizeEvent(QResizeEvent *event);

private slots:
	void updateRecentFileActions();

signals:
	void windowResized();

private:
	void init();
	void loadFile(const QString &fileName);
	void setCurrentFile(const QString &fileName);
	QString strippedName(const QString &fullFileName);

	QString curFile;
	QImage	image;
	
	Poppler::Document	*document;
	
	PDFWidget	*pdfWidget;
	QScrollArea	*scrollArea;

	TeXDocument *sourceDoc;

	QList<QAction*> recentFileActions;
	QMenu *menuRecent;
};

#endif
