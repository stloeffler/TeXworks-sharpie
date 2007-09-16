#include "TeXDocument.h"
#include "TeXHighlighter.h"
#include "FindDialog.h"
#include "QTeXApp.h"
#include "PDFDocument.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QStatusBar>
#include <QFontDialog>
#include <QInputDialog>
#include <QSettings>
#include <QStringList>
#include <QRegExp>

const int kStatusMessageDuration = 3000;
const int kNewWindowOffset = 32;

TeXDocument::TeXDocument()
{
	init();
	setCurrentFile("");
	statusBar()->showMessage(tr("New document"), kStatusMessageDuration);
}

TeXDocument::TeXDocument(const QString &fileName)
{
	init();
	loadFile(fileName);
}

TeXDocument::~TeXDocument()
{
	if (pdfDoc != NULL) {
		pdfDoc->close();
		pdfDoc = NULL;
	}
}

void
TeXDocument::init()
{
	pdfDoc = NULL;

	setupUi(this);
	
	setAttribute(Qt::WA_DeleteOnClose, true);
	setAttribute(Qt::WA_MacNoClickThrough, true);

	textEdit_console->hide();	

	lineNumberLabel = new QLabel();
	statusBar()->addPermanentWidget(lineNumberLabel);
	lineNumberLabel->setFrameStyle(QFrame::StyledPanel);
	lineNumberLabel->setFont(statusBar()->font());
	statusLine = statusTotal = 0;
	showCursorPosition();
	
	connect(actionNew, SIGNAL(triggered()), this, SLOT(newFile()));
	connect(actionOpen, SIGNAL(triggered()), this, SLOT(open()));
	connect(actionAbout_QTeX, SIGNAL(triggered()), qApp, SLOT(about()));

	connect(actionSave, SIGNAL(triggered()), this, SLOT(save()));
	connect(actionSave_As, SIGNAL(triggered()), this, SLOT(saveAs()));
	connect(actionClose, SIGNAL(triggered()), this, SLOT(close()));

	connect(actionFont, SIGNAL(triggered()), this, SLOT(doFontDialog()));
	connect(actionGo_to_Line, SIGNAL(triggered()), this, SLOT(doLineDialog()));
	connect(actionFind, SIGNAL(triggered()), this, SLOT(doFindDialog()));
	connect(actionFind_Again, SIGNAL(triggered()), this, SLOT(doFindAgain()));

	connect(actionCopy_to_Find, SIGNAL(triggered()), this, SLOT(copyToFind()));
	connect(actionCopy_to_Replace, SIGNAL(triggered()), this, SLOT(copyToReplace()));
	connect(actionFind_Selection, SIGNAL(triggered()), this, SLOT(findSelection()));

	connect(actionIndent, SIGNAL(triggered()), this, SLOT(doIndent()));
	connect(actionUnindent, SIGNAL(triggered()), this, SLOT(doUnindent()));

	connect(actionComment, SIGNAL(triggered()), this, SLOT(doComment()));
	connect(actionUncomment, SIGNAL(triggered()), this, SLOT(doUncomment()));

	connect(textEdit->document(), SIGNAL(modificationChanged(bool)), this, SLOT(setWindowModified(bool)));
	connect(textEdit, SIGNAL(cursorPositionChanged()), this, SLOT(showCursorPosition()));
	connect(textEdit, SIGNAL(selectionChanged()), this, SLOT(showCursorPosition()));

	menuRecent = new QMenu(tr("Open Recent"));
	updateRecentFileActions();
	menuFile->insertMenu(actionOpen_Recent, menuRecent);
	menuFile->removeAction(actionOpen_Recent);

	connect(qApp, SIGNAL(recentFileActionsChanged()), this, SLOT(updateRecentFileActions()));
	
	highlighter = new TeXHighlighter(textEdit->document());
}

void
TeXDocument::newFile()
{
	TeXDocument *doc = new TeXDocument;
	doc->move(x() + kNewWindowOffset, y() + kNewWindowOffset);
	doc->show();
	doc->raise();
	doc->activateWindow();
}

void
TeXDocument::open()
{
	QString fileName = QFileDialog::getOpenFileName();
	open(fileName);
}

void TeXDocument::open(const QString &fileName)
{
	TeXDocument *doc = NULL;
	if (!fileName.isEmpty()) {
		doc = findDocument(fileName);
		if (doc == NULL) {
			if (isUntitled && textEdit->document()->isEmpty() && !isWindowModified()) {
				loadFile(fileName);
				doc = this;
			}
			else {
				doc = new TeXDocument(fileName);
				if (doc->isUntitled) {
					delete doc;
					return;
				}
				doc->move(x() + kNewWindowOffset, y() + kNewWindowOffset);
			}
		}
	}
	if (doc != NULL) {
		doc->show();
		doc->raise();
		doc->activateWindow();
	}
}

void TeXDocument::closeEvent(QCloseEvent *event)
{
	if (maybeSave())
		event->accept();
	else
		event->ignore();
}

bool TeXDocument::save()
{
	if (isUntitled)
		return saveAs();
	else
		return saveFile(curFile);
}

bool TeXDocument::saveAs()
{
	QString fileName = QFileDialog::getSaveFileName(this);
	if (fileName.isEmpty())
		return false;

	return saveFile(fileName);
}

bool TeXDocument::maybeSave()
{
	if (textEdit->document()->isModified()) {
		QMessageBox::StandardButton ret;
		ret = QMessageBox::warning(this, tr("TeXWorks"),
					 tr("The document \"%1\" has been modified.\n"
						"Do you want to save your changes?")
						.arg(strippedName(curFile)),
					 QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
		if (ret == QMessageBox::Save)
			return save();
		else if (ret == QMessageBox::Cancel)
			return false;
	}
	return true;
}

void TeXDocument::loadFile(const QString &fileName)
{
	QFile file(fileName);
	if (!file.open(QFile::ReadOnly | QFile::Text)) {
		QMessageBox::warning(this, tr("TeXWorks"),
							 tr("Cannot read file \"%1\":\n%2.")
							 .arg(fileName)
							 .arg(file.errorString()));
		return;
	}

	QTextStream in(&file);
	in.setCodec("UTF-8");
	in.setAutoDetectUnicode(true);

	QApplication::setOverrideCursor(Qt::WaitCursor);
	textEdit->setPlainText(in.readAll());
	QApplication::restoreOverrideCursor();

	setCurrentFile(fileName);
	statusBar()->showMessage(tr("File \"%1\" loaded").arg(strippedName(curFile)), kStatusMessageDuration);

	QFileInfo fi(fileName);
	QString pdfName = fi.path() + "/" + fi.completeBaseName() + ".pdf";
	fi.setFile(pdfName);
	if (fi.exists()) {
		pdfDoc = new PDFDocument(pdfName, this);
		pdfDoc->show();
		connect(pdfDoc, SIGNAL(destroyed()), this, SLOT(pdfClosed()));
	}
}

void TeXDocument::pdfClosed()
{
	pdfDoc = NULL;
}

bool TeXDocument::saveFile(const QString &fileName)
{
	QFile file(fileName);
	if (!file.open(QFile::WriteOnly | QFile::Text)) {
		QMessageBox::warning(this, tr("TeXWorks"),
							 tr("Cannot write file \"%1\":\n%2.")
							 .arg(fileName)
							 .arg(file.errorString()));
		return false;
	}

	QTextStream out(&file);
	out.setCodec("UTF-8");
	QApplication::setOverrideCursor(Qt::WaitCursor);
	out << textEdit->toPlainText();
	QApplication::restoreOverrideCursor();

	setCurrentFile(fileName);
	statusBar()->showMessage(tr("File \"%1\" saved").arg(strippedName(curFile)), kStatusMessageDuration);
	return true;
}

void TeXDocument::setCurrentFile(const QString &fileName)
{
	static int sequenceNumber = 1;

	isUntitled = fileName.isEmpty();
	if (isUntitled)
		curFile = tr("untitled-%1.tex").arg(sequenceNumber++);
	else
		curFile = QFileInfo(fileName).canonicalFilePath();

	textEdit->document()->setModified(false);
	setWindowModified(false);

	setWindowTitle(tr("%1[*] - %2").arg(strippedName(curFile)).arg(tr("TeXWorks")));

	if (!isUntitled) {
		QSettings settings;
		QStringList files = settings.value("recentFileList").toStringList();
		files.removeAll(fileName);
		files.prepend(fileName);
		while (files.size() > kMaxRecentFiles)
			files.removeLast();
		settings.setValue("recentFileList", files);
		QTeXApp *app = qobject_cast<QTeXApp*>(qApp);
		if (app)
			app->updateRecentFileActions();
	}
}

void TeXDocument::updateRecentFileActions()
{
	QTeXApp::updateRecentFileActions(this, recentFileActions, menuRecent);
}

QString TeXDocument::strippedName(const QString &fullFileName)
{
	return QFileInfo(fullFileName).fileName();
}

void TeXDocument::showCursorPosition()
{
	int line = textEdit->textCursor().blockNumber() + 1;
	int total = textEdit->document()->blockCount();
	if (line != statusLine || total != statusTotal) {
		lineNumberLabel->setText(tr("Line %1 of %2").arg(line).arg(total));
		statusLine = line;
		statusTotal = total;
	}
}

TeXDocument *TeXDocument::findDocument(const QString &fileName)
{
	QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

	foreach (QWidget *widget, qApp->topLevelWidgets()) {
		TeXDocument *theDoc = qobject_cast<TeXDocument*>(widget);
		if (theDoc && theDoc->curFile == canonicalFilePath)
			return theDoc;
	}
	return NULL;
}

void TeXDocument::doFontDialog()
{
	textEdit->setFont(QFontDialog::getFont(0, textEdit->font()));
}

void TeXDocument::doLineDialog()
{
	QTextCursor cursor = textEdit->textCursor();

	bool ok;
	int i = QInputDialog::getInteger(this, tr("Go to Line"),
									tr("Line number:"), cursor.blockNumber() + 1,
									1, textEdit->document()->blockCount(), 1, &ok);
	if (ok) {
		cursor.setPosition(0);
		cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, i - 1);
		cursor.select(QTextCursor::BlockUnderCursor);
		textEdit->setTextCursor(cursor);
	}
}

void TeXDocument::doFindDialog()
{
	FindDialog::doFindDialog(this);
}

void TeXDocument::prefixLines(const QString &prefix)
{
	QTextCursor cursor = textEdit->textCursor();
	cursor.beginEditBlock();
	int selStart = cursor.selectionStart();
	int selEnd = cursor.selectionEnd();
	cursor.setPosition(selStart);
	if (!cursor.atBlockStart()) {
		cursor.movePosition(QTextCursor::StartOfBlock);
		selStart = cursor.position();
	}
	cursor.setPosition(selEnd);
	if (!cursor.atBlockStart() || selEnd == selStart) {
		cursor.movePosition(QTextCursor::NextBlock);
		selEnd = cursor.position();
	}
	if (selEnd == selStart)
		goto handle_end_of_doc;	// special case
	while (cursor.position() > selStart) {
		cursor.movePosition(QTextCursor::PreviousBlock);
	handle_end_of_doc:
		cursor.insertText(prefix);
		cursor.movePosition(QTextCursor::StartOfBlock);
		selEnd += prefix.length();
	}
	cursor.setPosition(selStart);
	cursor.setPosition(selEnd, QTextCursor::KeepAnchor);
	textEdit->setTextCursor(cursor);
	cursor.endEditBlock();
}

void TeXDocument::doIndent()
{
	prefixLines("\t");
}

void TeXDocument::doComment()
{
	prefixLines("%");
}

void TeXDocument::unPrefixLines(const QString &prefix)
{
	QTextCursor cursor = textEdit->textCursor();
	cursor.beginEditBlock();
	int selStart = cursor.selectionStart();
	int selEnd = cursor.selectionEnd();
	cursor.setPosition(selStart);
	if (!cursor.atBlockStart()) {
		cursor.movePosition(QTextCursor::StartOfBlock);
		selStart = cursor.position();
	}
	cursor.setPosition(selEnd);
	if (!cursor.atBlockStart() || selEnd == selStart) {
		cursor.movePosition(QTextCursor::NextBlock);
		selEnd = cursor.position();
	}
	while (cursor.position() > selStart) {
		cursor.movePosition(QTextCursor::PreviousBlock);
		cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
		QString		str = cursor.selectedText();
		if (str == prefix) {
			cursor.removeSelectedText();
			selEnd -= prefix.length();
		}
		else
			cursor.movePosition(QTextCursor::PreviousCharacter);
	}
	cursor.setPosition(selStart);
	cursor.setPosition(selEnd, QTextCursor::KeepAnchor);
	textEdit->setTextCursor(cursor);
	cursor.endEditBlock();
}

void TeXDocument::doUnindent()
{
	unPrefixLines("\t");
}

void TeXDocument::doUncomment()
{
	unPrefixLines("%");
}

void TeXDocument::doFindAgain()
{
	QSettings settings;
	QString	searchText = settings.value("searchText").toString();
	QTextDocument::FindFlags flags = (QTextDocument::FindFlags)settings.value("searchFlags").toInt();
	QTextCursor	curs;
	if (settings.value("searchRegex").toBool()) {
		QRegExp	regex(searchText,
						(flags & QTextDocument::FindCaseSensitively) != 0
						? Qt::CaseSensitive : Qt::CaseInsensitive);
		if (regex.isValid()) {
			curs = textEdit->document()->find(regex, textEdit->textCursor(), flags);
			if (curs.isNull() && settings.value("searchWrap").toBool()) {
				curs = QTextCursor(textEdit->document());
				if ((flags & QTextDocument::FindBackward) != 0)
					curs.movePosition(QTextCursor::End);
				curs = textEdit->document()->find(regex, curs, flags);
			}
		}
		else {
			qApp->beep();
			statusBar()->showMessage(tr("Invalid regular expression"), kStatusMessageDuration);
			return;
		}
	}
	else {
		curs = textEdit->document()->find(searchText, textEdit->textCursor(), flags);
		if (curs.isNull() && settings.value("searchWrap").toBool()) {
			curs = QTextCursor(textEdit->document());
			if ((flags & QTextDocument::FindBackward) != 0)
				curs.movePosition(QTextCursor::End);
			curs = textEdit->document()->find(searchText, curs, flags);
		}
	}
	if (curs.isNull()) {
		qApp->beep();
		statusBar()->showMessage(tr("Not found"), kStatusMessageDuration);
	}
	else {
		textEdit->setTextCursor(curs);
	}
}

void TeXDocument::copyToFind()
{
	if (textEdit->textCursor().hasSelection()) {
		QString searchText = textEdit->textCursor().selectedText();
		QSettings settings;
		if (settings.value("searchRegex").toBool())
			searchText = QRegExp::escape(searchText);
		settings.setValue("searchText", searchText);
	}
}

void TeXDocument::copyToReplace()
{
	if (textEdit->textCursor().hasSelection()) {
		QString replaceText = textEdit->textCursor().selectedText();
		QSettings settings;
		settings.setValue("replaceText", replaceText);
	}
}

void TeXDocument::findSelection()
{
	copyToFind();
	doFindAgain();
}
