/*
 This is part of TeXworks, an environment for working with TeX documents
 Copyright (C) 2007-2010  Stefan Löffler & Jonathan Kew
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 
 For links to further information, or to contact the author,
 see <http://texworks.org/>.
*/

#ifndef TWScriptAPI_H
#define TWScriptAPI_H

#include "TWScript.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QMessageBox>

class TWScriptAPI : public QObject
{
	Q_OBJECT
	
    Q_PROPERTY(QObject* app READ GetApp);
	Q_PROPERTY(QObject* target READ GetTarget);
	Q_PROPERTY(QVariant result READ GetResult WRITE SetResult);
	Q_PROPERTY(QObject * script READ GetScript);
	
public:
	TWScriptAPI(TWScript* script, QObject* twapp, QObject* ctx, QVariant& res);
	
public:
	QObject* GetApp() { return m_app; }
	QObject* GetTarget() { return m_target; }
	QObject* GetScript() { return m_script; }
	QVariant& GetResult() { return m_result; }
	
	void SetResult(const QVariant& rval);
	
	// provide utility functions for scripts, implemented as methods on the TW object

	// length of a string in UTF-16 code units, useful if script language uses a different encoding form
	Q_INVOKABLE
	int strlen(const QString& str) const;
	
	// return the host platform name
	Q_INVOKABLE
	QString platform() const;
	
	// return the Qt version Tw was built against (0xMMNNPP)
	Q_INVOKABLE
	int getQtVersion() const { return QT_VERSION; }

	// QMessageBox functions to display alerts
	Q_INVOKABLE
	int information(QWidget* parent,
					const QString& title, const QString& text,
					int buttons = (int)QMessageBox::Ok,
					int defaultButton = QMessageBox::NoButton);
	Q_INVOKABLE
	int question(QWidget* parent,
				 const QString& title, const QString& text,
				 int buttons = (int)QMessageBox::Ok,
				 int defaultButton = QMessageBox::NoButton);
	Q_INVOKABLE
	int warning(QWidget* parent,
				const QString& title, const QString& text,
				int buttons = (int)QMessageBox::Ok,
				int defaultButton = QMessageBox::NoButton);
	Q_INVOKABLE
	int critical(QWidget* parent,
				 const QString& title, const QString& text,
				 int buttons = (int)QMessageBox::Ok,
				 int defaultButton = QMessageBox::NoButton);
	
	// QInputDialog functions
	// These return QVariant rather than simple types, so that they can return null
	// to indicate that the dialog was cancelled.
	Q_INVOKABLE
	QVariant getInt(QWidget* parent, const QString& title, const QString& label,
					int value = 0, int min = -2147483647, int max = 2147483647, int step = 1);
	Q_INVOKABLE
	QVariant getDouble(QWidget* parent, const QString& title, const QString& label,
					   double value = 0, double min = -2147483647, double max = 2147483647, int decimals = 1);
	Q_INVOKABLE
	QVariant getItem(QWidget* parent, const QString& title, const QString& label,
					 const QStringList& items, int current = 0, bool editable = true);
	Q_INVOKABLE
	QVariant getText(QWidget* parent, const QString& title, const QString& label,
					 const QString& text = QString());
	
	Q_INVOKABLE
	void yield();
	
	// Allow script to create a QProgressDialog
	Q_INVOKABLE
	QWidget * progressDialog(QWidget * parent);
	
	// functions to create windows from .ui data or files using QUiLoader
	Q_INVOKABLE
	QWidget * createUIFromString(const QString& uiSpec, QWidget * parent = NULL);

	Q_INVOKABLE
	QWidget * createUI(const QString& filename, QWidget * parent = NULL);
	
	// to find children of a widget
	Q_INVOKABLE
	QWidget * findChildWidget(QWidget* parent, const QString& name);
	
	// to make connections among widgets (or other objects)
	Q_INVOKABLE
	bool makeConnection(QObject* sender, const QString& signal, QObject* receiver, const QString& slot);
	

protected:
	TWScript* m_script;
	QObject* m_app;
	QObject* m_target;
	QVariant& m_result;
};

#endif /* TWScriptAPI_H */
