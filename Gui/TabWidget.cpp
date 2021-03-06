//  Natron
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/*
 * Created by Alexandre GAUTHIER-FOICHAT on 6/1/2012.
 * contact: immarespond at gmail dot com
 *
 */

// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>

#include "TabWidget.h"

#include "Global/Macros.h"
CLANG_DIAG_OFF(deprecated)
#include <QLayout>
#include <QMenu>
#include <QApplication>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtGui/QIcon>
#include <QtCore/QMimeData>
#include <QtGui/QDrag>
#include <QDebug>
CLANG_DIAG_OFF(unused-private-field)
// /opt/local/include/QtGui/qmime.h:119:10: warning: private field 'type' is not used [-Wunused-private-field]
#include <QtGui/QDragEnterEvent>
CLANG_DIAG_ON(unused-private-field)
#include <QtGui/QDragLeaveEvent>
#include <QtGui/QPaintEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QTextDocument> // for Qt::convertFromPlainText
CLANG_DIAG_ON(deprecated)

#include "Gui/Button.h"
#include "Gui/GuiApplicationManager.h"
#include "Gui/GuiAppInstance.h"
#include "Gui/Gui.h"
#include "Gui/NodeGraph.h"
#include "Gui/CurveEditor.h"
#include "Gui/ViewerTab.h"
#include "Gui/Histogram.h"
#include "Gui/Splitter.h"
#include "Gui/GuiMacros.h"
#include "Gui/ActionShortcuts.h"
#include "Gui/MenuWithToolTips.h"
#include "Gui/ScriptEditor.h"
#include "Gui/PythonPanels.h"

#include "Engine/ViewerInstance.h"
#include "Engine/Project.h"
#include "Engine/ScriptObject.h"

#define LEFT_HAND_CORNER_BUTTON_TT "Manage the layouts for this pane"

using namespace Natron;

struct TabWidgetPrivate
{
    TabWidget* _publicInterface;
    Gui* gui;
    QVBoxLayout* mainLayout;
    std::vector<std::pair<QWidget*,ScriptObject*> > tabs; // the actual tabs
    QWidget* header;
    QHBoxLayout* headerLayout;
    bool modifyingTabBar;
    TabBar* tabBar; // the header containing clickable pages
    Button* leftCornerButton;
    Button* floatButton;
    Button* closeButton;
    QWidget* currentWidget;
    bool drawDropRect;
    bool fullScreen;
    bool isAnchor;
    bool tabBarVisible;
    
    ///Protects  currentWidget, fullScreen, isViewerAnchor
    mutable QMutex tabWidgetStateMutex;
    
    TabWidgetPrivate(TabWidget* publicInterface,Gui* gui)
    : _publicInterface(publicInterface)
    , gui(gui)
    , mainLayout(0)
    , tabs()
    , header(0)
    , headerLayout(0)
    , modifyingTabBar(false)
    , tabBar(0)
    , leftCornerButton(0)
    , floatButton(0)
    , closeButton(0)
    , currentWidget(0)
    , drawDropRect(false)
    , fullScreen(false)
    , isAnchor(false)
    , tabBarVisible(true)
    , tabWidgetStateMutex()
    {
        

    }
    
    void declareTabToPython(QWidget* widget,const std::string& tabName);
    void removeTabToPython(QWidget* widget,const std::string& tabName);
};

TabWidget::TabWidget(Gui* gui,
                     QWidget* parent)
    : QFrame(parent)
    , _imp(new TabWidgetPrivate(this,gui))
{
    setMouseTracking(true);
    setFrameShape(QFrame::NoFrame);
    _imp->mainLayout = new QVBoxLayout(this);
    _imp->mainLayout->setContentsMargins(0, 5, 0, 0);
    _imp->mainLayout->setSpacing(0);

    _imp->header = new TabWidgetHeader(this);
    QObject::connect( _imp->header, SIGNAL(mouseLeftTabBar()), this, SLOT(onTabBarMouseLeft()));
    _imp->headerLayout = new QHBoxLayout(_imp->header);
    _imp->headerLayout->setContentsMargins(0, 0, 0, 0);
    _imp->headerLayout->setSpacing(0);
    _imp->header->setLayout(_imp->headerLayout);


    QPixmap pixC,pixM,pixL;
    appPTR->getIcon(NATRON_PIXMAP_CLOSE_WIDGET,&pixC);
    appPTR->getIcon(NATRON_PIXMAP_MAXIMIZE_WIDGET,&pixM);
    appPTR->getIcon(NATRON_PIXMAP_TAB_WIDGET_LAYOUT_BUTTON,&pixL);


    _imp->leftCornerButton = new Button(QIcon(pixL),"", _imp->header);
    _imp->leftCornerButton->setFixedSize(NATRON_SMALL_BUTTON_SIZE,NATRON_SMALL_BUTTON_SIZE);
    _imp->leftCornerButton->setToolTip( Qt::convertFromPlainText(tr(LEFT_HAND_CORNER_BUTTON_TT), Qt::WhiteSpaceNormal) );
    _imp->leftCornerButton->setFocusPolicy(Qt::NoFocus);
    _imp->headerLayout->addWidget(_imp->leftCornerButton);
    _imp->headerLayout->addSpacing(10);

    _imp->tabBar = new TabBar(this,_imp->header);
    _imp->tabBar->setShape(QTabBar::RoundedNorth);
    _imp->tabBar->setDrawBase(false);
    QObject::connect( _imp->tabBar, SIGNAL( currentChanged(int) ), this, SLOT( makeCurrentTab(int) ) );
    QObject::connect( _imp->tabBar, SIGNAL(mouseLeftTabBar()), this, SLOT(onTabBarMouseLeft()));
    _imp->headerLayout->addWidget(_imp->tabBar);
    _imp->headerLayout->addStretch();
    _imp->floatButton = new Button(QIcon(pixM),"",_imp->header);
    _imp->floatButton->setFixedSize(NATRON_SMALL_BUTTON_SIZE,NATRON_SMALL_BUTTON_SIZE);
    _imp->floatButton->setToolTip( Qt::convertFromPlainText(tr("Float pane"), Qt::WhiteSpaceNormal) );
    _imp->floatButton->setEnabled(true);
    _imp->floatButton->setFocusPolicy(Qt::NoFocus);
    QObject::connect( _imp->floatButton, SIGNAL( clicked() ), this, SLOT( floatCurrentWidget() ) );
    _imp->headerLayout->addWidget(_imp->floatButton);

    _imp->closeButton = new Button(QIcon(pixC),"",_imp->header);
    _imp->closeButton->setFixedSize(NATRON_SMALL_BUTTON_SIZE,NATRON_SMALL_BUTTON_SIZE);
    _imp->closeButton->setToolTip( Qt::convertFromPlainText(tr("Close pane"), Qt::WhiteSpaceNormal) );
    _imp->closeButton->setFocusPolicy(Qt::NoFocus);
    QObject::connect( _imp->closeButton, SIGNAL( clicked() ), this, SLOT( closePane() ) );
    _imp->headerLayout->addWidget(_imp->closeButton);


    /*adding menu to the left corner button*/
    _imp->leftCornerButton->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect( _imp->leftCornerButton, SIGNAL( clicked() ), this, SLOT( createMenu() ) );


    _imp->mainLayout->addWidget(_imp->header);
    _imp->mainLayout->addStretch();
}

TabWidget::~TabWidget()
{
}

Gui*
TabWidget::getGui() const
{
    return _imp->gui;
}

int
TabWidget::count() const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);
    
    return _imp->tabs.size();
}

QWidget*
TabWidget::tabAt(int index) const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);
    if (index < 0 || index >= (int)_imp->tabs.size()) {
        return 0;
    }
    return _imp->tabs[index].first;
}

void
TabWidget::tabAt(int index, QWidget** w, ScriptObject** obj) const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);
    if (index < 0 || index >= (int)_imp->tabs.size()) {
        *w = 0;
        *obj = 0;
        return ;
    }
    *w = _imp->tabs[index].first;
    *obj = _imp->tabs[index].second;
}


void
TabWidget::notifyGuiAboutRemoval()
{
    _imp->gui->unregisterPane(this);
}

void
TabWidget::setClosable(bool closable)
{
    _imp->closeButton->setEnabled(closable);
    _imp->floatButton->setEnabled(closable);
}


void
TabWidget::createMenu()
{
    MenuWithToolTips menu(_imp->gui);
    QFont f(appFont,appFontSize);
    menu.setFont(f) ;
    QPixmap pixV,pixM,pixH,pixC,pixA;
    appPTR->getIcon(NATRON_PIXMAP_TAB_WIDGET_SPLIT_VERTICALLY,&pixV);
    appPTR->getIcon(NATRON_PIXMAP_TAB_WIDGET_SPLIT_HORIZONTALLY,&pixH);
    appPTR->getIcon(NATRON_PIXMAP_MAXIMIZE_WIDGET,&pixM);
    appPTR->getIcon(NATRON_PIXMAP_CLOSE_WIDGET,&pixC);
    appPTR->getIcon(NATRON_PIXMAP_TAB_WIDGET_LAYOUT_BUTTON_ANCHOR,&pixA);
    QAction* splitVerticallyAction = new QAction(QIcon(pixV),tr("Split vertical"),&menu);
    QObject::connect( splitVerticallyAction, SIGNAL( triggered() ), this, SLOT( onSplitVertically() ) );
    menu.addAction(splitVerticallyAction);
    QAction* splitHorizontallyAction = new QAction(QIcon(pixH),tr("Split horizontal"),&menu);
    QObject::connect( splitHorizontallyAction, SIGNAL( triggered() ), this, SLOT( onSplitHorizontally() ) );
    menu.addAction(splitHorizontallyAction);
    menu.addSeparator();
    QAction* floatAction = new QAction(QIcon(pixM),tr("Float pane"),&menu);
    QObject::connect( floatAction, SIGNAL( triggered() ), this, SLOT( floatCurrentWidget() ) );
    menu.addAction(floatAction);


    if ( (_imp->tabBar->count() == 0) ) {
        floatAction->setEnabled(false);
    }

    QAction* closeAction = new QAction(QIcon(pixC),tr("Close pane"), &menu);
    closeAction->setEnabled( _imp->closeButton->isEnabled() );
    QObject::connect( closeAction, SIGNAL( triggered() ), this, SLOT( closePane() ) );
    menu.addAction(closeAction);
    
    QAction* hideToolbar;
    if (_imp->gui->isLeftToolBarDisplayedOnMouseHoverOnly()) {
        hideToolbar = new QAction(tr("Show left toolbar"),&menu);
    } else {
        hideToolbar = new QAction(tr("Hide left toolbar"),&menu);
    }
    QObject::connect(hideToolbar, SIGNAL(triggered()), this, SLOT(onHideLeftToolBarActionTriggered()));
    menu.addAction(hideToolbar);
    
    QAction* hideTabbar;
    if (_imp->tabBarVisible) {
        hideTabbar = new QAction(tr("Hide tabs header"),&menu);
    } else {
        hideTabbar = new QAction(tr("Show tabs header"),&menu);
    }
    QObject::connect(hideTabbar, SIGNAL(triggered()), this, SLOT(onShowHideTabBarActionTriggered()));
    menu.addAction(hideTabbar);
    menu.addSeparator();
    menu.addAction( tr("New viewer"), this, SLOT( addNewViewer() ) );
    menu.addAction( tr("New histogram"), this, SLOT( newHistogramHere() ) );
    menu.addAction( tr("Node graph here"), this, SLOT( moveNodeGraphHere() ) );
    menu.addAction( tr("Curve Editor here"), this, SLOT( moveCurveEditorHere() ) );
    menu.addAction( tr("Properties bin here"), this, SLOT( movePropertiesBinHere() ) );
    menu.addAction( tr("Script editor here"), this, SLOT( moveScriptEditorHere() ) );
    
    
    std::map<PyPanel*,std::string> userPanels = _imp->gui->getPythonPanels();
    if (!userPanels.empty()) {
        QMenu* userPanelsMenu = new QMenu(tr("User panels"),&menu);
        userPanelsMenu->setFont(f);
        menu.addAction(userPanelsMenu->menuAction());
        
        
        for (std::map<PyPanel*,std::string>::iterator it = userPanels.begin(); it != userPanels.end(); ++it) {
            QAction* pAction = new QAction(QString(it->first->getPanelLabel().c_str()) + tr(" here"),userPanelsMenu);
            QObject::connect(pAction, SIGNAL(triggered()), this, SLOT(onUserPanelActionTriggered()));
            pAction->setData(it->first->getScriptName().c_str());
            userPanelsMenu->addAction(pAction);
        }
    }
    menu.addSeparator();
    
    QAction* isAnchorAction = new QAction(QIcon(pixA),tr("Set this as anchor"),&menu);
    isAnchorAction->setToolTip(tr("The anchor pane is where viewers will be created by default."));
    isAnchorAction->setCheckable(true);
    bool isVA = isAnchor();
    isAnchorAction->setChecked(isVA);
    isAnchorAction->setEnabled(!isVA);
    QObject::connect( isAnchorAction, SIGNAL( triggered() ), this, SLOT( onSetAsAnchorActionTriggered() ) );
    menu.addAction(isAnchorAction);

    menu.exec( _imp->leftCornerButton->mapToGlobal( QPoint(0,0) ) );
}

void
TabWidget::onUserPanelActionTriggered()
{
    QAction* s = qobject_cast<QAction*>(sender());
    if (!s) {
        return;
    }
    
    const RegisteredTabs& tabs = _imp->gui->getRegisteredTabs();
    RegisteredTabs::const_iterator found = tabs.find(s->data().toString().toStdString());
    if (found != tabs.end()) {
        moveTab(found->second.first, found->second.second, this);
    }
}

void
TabWidget::onHideLeftToolBarActionTriggered()
{
    _imp->gui->setLeftToolBarDisplayedOnMouseHoverOnly(!_imp->gui->isLeftToolBarDisplayedOnMouseHoverOnly());
}

void
TabWidget::moveToNextTab()
{
    int nextTab = -1;
    {
        QMutexLocker l(&_imp->tabWidgetStateMutex);
        
        for (U32 i = 0; i < _imp->tabs.size(); ++i) {
            if (_imp->tabs[i].first == _imp->currentWidget) {
                if (i == _imp->tabs.size() -1) {
                    nextTab = 0;
                } else {
                    nextTab = i + 1;
                }
            }
        }
    }
    if (nextTab != -1) {
        makeCurrentTab(nextTab);
    }
}

void
TabWidget::tryCloseFloatingPane()
{
    FloatingWidget* parent = dynamic_cast<FloatingWidget*>( parentWidget() );

    if (parent) {
        parent->close();
    }
}

static void
getOtherSplitWidget(Splitter* parentSplitter,
                    QWidget* thisWidget,
                    QWidget* & other)
{
    for (int i = 0; i < parentSplitter->count(); ++i) {
        QWidget* w = parentSplitter->widget(i);
        if (w == thisWidget) {
            continue;
        }
        other = w;
        break;
    }
}

/**
 * @brief Deletes container and move the other split (the one that is not this) to the parent of container.
 * If the parent container is a splitter then we will insert it instead of the container.
 * Otherwise if the parent is a floating window we will insert it as embedded widget of the window.
 **/
void
TabWidget::closeSplitterAndMoveOtherSplitToParent(Splitter* container)
{
    QWidget* otherSplit = 0;

    /*identifying the other split*/
    getOtherSplitWidget(container,this,otherSplit);
    assert(otherSplit);

    Splitter* parentSplitter = dynamic_cast<Splitter*>( container->parentWidget() );
    FloatingWidget* parentWindow = dynamic_cast<FloatingWidget*>( container->parentWidget() );

    assert(parentSplitter || parentWindow);

    if (parentSplitter) {
        QList<int> mainContainerSizes = parentSplitter->sizes();

        /*Removing the splitter container from its parent*/
        int containerIndexInParentSplitter = parentSplitter->indexOf(container);
        parentSplitter->removeChild_mt_safe(container);

        /*moving the other split to the mainContainer*/
        parentSplitter->insertChild_mt_safe(containerIndexInParentSplitter, otherSplit);
        otherSplit->setVisible(true);

        /*restore the main container sizes*/
        parentSplitter->setSizes_mt_safe(mainContainerSizes);
    } else {
        assert(parentWindow);
        parentWindow->removeEmbeddedWidget();
        parentWindow->setWidget(otherSplit);
    }

    /*Remove the container from everywhere*/
    _imp->gui->unregisterSplitter(container);
    container->setParent(NULL);
    container->deleteLater();
}

void
TabWidget::closePane()
{
    if (!_imp->gui) {
        return;
    }

    FloatingWidget* isFloating = dynamic_cast<FloatingWidget*>( parentWidget() );
    if ( isFloating && (isFloating->getEmbeddedWidget() == this) ) {
        isFloating->close();
    }


    /*Removing it from the _panes vector*/

    _imp->gui->unregisterPane(this);


    ///This is the TabWidget to which we will move all our splits.
    TabWidget* tabToTransferTo = 0;

    ///Move living tabs to the viewer anchor TabWidget, this is better than destroying them.
    const std::list<TabWidget*> & panes = _imp->gui->getPanes();
    for (std::list<TabWidget*>::const_iterator it = panes.begin(); it != panes.end(); ++it) {
        if ( (*it != this) && (*it)->isAnchor() ) {
            tabToTransferTo = *it;
            break;
        }
    }


    if (tabToTransferTo) {
        ///move this tab's splits
        while (count() > 0) {
            QWidget* w;
            ScriptObject* o;
            tabAt(0,&w,&o);
            if (w && o) {
                moveTab(w, o, tabToTransferTo);
            }
        }
    } else {
        while (count() > 0) {
            QWidget* w = tabAt(0);
            if (w) {
                removeTab( w, true );
            }
        }
    }

    ///Hide this
    setVisible(false);

    Splitter* container = dynamic_cast<Splitter*>( parentWidget() );
    if (container) {
        closeSplitterAndMoveOtherSplitToParent(container);
    }
} // closePane

void
TabWidget::floatPane(QPoint* position)
{
    FloatingWidget* isParentFloating = dynamic_cast<FloatingWidget*>( parentWidget() );

    if (isParentFloating) {
        return;
    }

    FloatingWidget* floatingW = new FloatingWidget(_imp->gui,_imp->gui);
    Splitter* parentSplitter = dynamic_cast<Splitter*>( parentWidget() );
    setParent(0);
    if (parentSplitter) {
        closeSplitterAndMoveOtherSplitToParent(parentSplitter);
    }


    floatingW->setWidget(this);

    if (position) {
        floatingW->move(*position);
    }
    _imp->gui->registerFloatingWindow(floatingW);
    _imp->gui->checkNumberOfNonFloatingPanes();
}

void
TabWidget::addNewViewer()
{
    _imp->gui->setNextViewerAnchor(this);
    
    NodeGraph* lastFocusedGraph = _imp->gui->getLastSelectedGraph();
    NodeGraph* graph = lastFocusedGraph ? lastFocusedGraph : _imp->gui->getNodeGraph();
    assert(graph);
    _imp->gui->getApp()->createNode(  CreateNodeArgs(PLUGINID_NATRON_VIEWER,
                                                "",
                                                -1,-1,
                                                true,
                                                INT_MIN,INT_MIN,
                                                true,
                                                true,
                                                QString(),
                                                CreateNodeArgs::DefaultValuesList(),
                                                graph->getGroup()) );
}

void
TabWidget::moveNodeGraphHere()
{
    NodeGraph* graph = _imp->gui->getNodeGraph();

    moveTab(graph, graph ,this);
}

void
TabWidget::moveCurveEditorHere()
{
    CurveEditor* editor = _imp->gui->getCurveEditor();

    moveTab(editor, editor,this);
}

void
TabWidget::newHistogramHere()
{
    Histogram* h = _imp->gui->addNewHistogram();

    appendTab(h,h);

    _imp->gui->getApp()->triggerAutoSave();
}

/*Get the header name of the tab at index "index".*/
QString
TabWidget::getTabLabel(int index) const
{
    QMutexLocker k(&_imp->tabWidgetStateMutex);
    if (index < 0 || index >= _imp->tabBar->count() ) {
        return QString();
    }
    return _imp->tabs[index].second->getLabel().c_str();
}

QString
TabWidget::getTabLabel(QWidget* tab) const
{
    QMutexLocker k(&_imp->tabWidgetStateMutex);
    for (U32 i = 0; i < _imp->tabs.size(); ++i) {
        if (_imp->tabs[i].first == tab) {
            return _imp->tabs[i].second->getLabel().c_str();
        }
    }
    return QString();
}

void
TabWidget::setTabLabel(QWidget* tab,
                      const QString & name)
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);

    for (U32 i = 0; i < _imp->tabs.size(); ++i) {
        if (_imp->tabs[i].first == tab) {
            _imp->tabBar->setTabText(i, name);
        }
    }
}

void
TabWidget::floatCurrentWidget()
{
    if (_imp->tabs.empty()) {
        return;
    }
    if ( !_imp->closeButton->isEnabled() ) {
        ///Make a new tab widget and float it instead
        TabWidget* newPane = new TabWidget(_imp->gui,_imp->gui);
        _imp->gui->registerPane(newPane);
        newPane->setObjectName_mt_safe( _imp->gui->getAvailablePaneName() );
        
        QWidget* w;
        ScriptObject* o;
        currentWidget(&w, &o);
        if (w && o) {
            moveTab(w, o, newPane);
        }
        newPane->floatPane();
    } else {
        ///Float this tab
        floatPane();
    }
}

void
TabWidget::closeCurrentWidget()
{
    QWidget* w = currentWidget();
    if (!w) {
        return;
    }
    removeTab(w,true);
    
}

void
TabWidget::closeTab(int index)
{
    
    QWidget *w = tabAt(index);
    if (!w) {
        return;
    }
    removeTab(w, true);
    
    _imp->gui->getApp()->triggerAutoSave();
}

void
TabWidget::movePropertiesBinHere()
{
    moveTab(_imp->gui->getPropertiesBin(), _imp->gui->getPropertiesBin(), this);
}

void
TabWidget::moveScriptEditorHere()
{
    moveTab(_imp->gui->getScriptEditor(), _imp->gui->getScriptEditor(), this);
}

TabWidget*
TabWidget::splitInternal(bool autoSave,
                         Qt::Orientation orientation)
{
    FloatingWidget* parentIsFloating = dynamic_cast<FloatingWidget*>( parentWidget() );
    Splitter* parentIsSplitter = dynamic_cast<Splitter*>( parentWidget() );

    assert(parentIsSplitter || parentIsFloating);

    /*We need to know the position in the container layout of the old tab widget*/
    int oldIndexInParentSplitter;
    QList<int> oldSizeInParentSplitter;
    if (parentIsSplitter) {
        oldIndexInParentSplitter = parentIsSplitter->indexOf(this);
        oldSizeInParentSplitter = parentIsSplitter->sizes();
    } else {
        assert(parentIsFloating);
        parentIsFloating->removeEmbeddedWidget();
    }

    Splitter* newSplitter = new Splitter;
    newSplitter->setContentsMargins(0, 0, 0, 0);
    newSplitter->setOrientation(orientation);
    _imp->gui->registerSplitter(newSplitter);
 
    /*Add this to the new splitter*/
    newSplitter->addWidget_mt_safe(this);

    /*Adding now a new TabWidget*/
    TabWidget* newTab = new TabWidget(_imp->gui,newSplitter);
    _imp->gui->registerPane(newTab);
    newTab->setObjectName_mt_safe( _imp->gui->getAvailablePaneName() );
    newSplitter->insertChild_mt_safe(-1,newTab);

    /*Resize the whole thing so each split gets the same size*/
    QSize splitterSize;
    if (parentIsSplitter) {
        splitterSize = newSplitter->sizeHint();
    } else {
        splitterSize = parentIsFloating->size();
    }

    QList<int> sizes;
    if (orientation == Qt::Vertical) {
        sizes << splitterSize.height() / 2;
        sizes << splitterSize.height() / 2;
    } else if (orientation == Qt::Horizontal) {
        sizes << splitterSize.width() / 2;
        sizes << splitterSize.width() / 2;
    } else {
        assert(false);
    }
    newSplitter->setSizes_mt_safe(sizes);

    if (parentIsFloating) {
        parentIsFloating->setWidget(newSplitter);
        parentIsFloating->resize(splitterSize);
    } else {
        /*The parent MUST be a splitter otherwise there's a serious bug!*/
        assert(parentIsSplitter);

        /*Inserting back the new splitter at the original index*/
        parentIsSplitter->insertChild_mt_safe(oldIndexInParentSplitter,newSplitter);

        /*restore the container original sizes*/
        parentIsSplitter->setSizes_mt_safe(oldSizeInParentSplitter);
    }


    if (!_imp->gui->getApp()->getProject()->isLoadingProject() && autoSave) {
        _imp->gui->getApp()->triggerAutoSave();
    }

    return newTab;
} // splitInternal

TabWidget*
TabWidget::splitHorizontally(bool autoSave)
{
    return splitInternal(autoSave, Qt::Horizontal);
}

TabWidget*
TabWidget::splitVertically(bool autoSave)
{
    return splitInternal(autoSave, Qt::Vertical);
}

bool
TabWidget::appendTab(QWidget* widget, ScriptObject* object)
{
    return appendTab(QIcon(),widget,object);
}

bool
TabWidget::appendTab(const QIcon & icon,
                     QWidget* widget,
                    ScriptObject* object)
{
    {
        QMutexLocker l(&_imp->tabWidgetStateMutex);

        ///If we do not know the tab ignore it
        std::string name = object->getScriptName();
        std::string label = object->getLabel();
        if ( name.empty() || label.empty() ) {
            return false;
        }

        /*registering this tab for drag&drop*/
        _imp->gui->registerTab(widget,object);

        _imp->tabs.push_back(std::make_pair(widget,object));
        widget->setParent(this);
        _imp->modifyingTabBar = true;
        _imp->tabBar->addTab(icon,label.c_str());
        _imp->tabBar->updateGeometry(); //< necessary
        _imp->modifyingTabBar = false;
        if (_imp->tabs.size() == 1) {
            for (int i = 0; i < _imp->mainLayout->count(); ++i) {
                QSpacerItem* item = dynamic_cast<QSpacerItem*>( _imp->mainLayout->itemAt(i) );
                if (item) {
                    _imp->mainLayout->removeItem(item);
                }
            }
        }
        if (!widget->isVisible()) {
            widget->setVisible(true);
        }
        _imp->floatButton->setEnabled(true);
        
    }
    _imp->declareTabToPython(widget, object->getScriptName());
    makeCurrentTab(_imp->tabs.size() - 1);

    return true;
}

void
TabWidget::insertTab(int index,
                     const QIcon & icon,
                     QWidget* widget,
                      ScriptObject* object)
{

    QString title = object->getLabel().c_str();

    QMutexLocker l(&_imp->tabWidgetStateMutex);

    if ( (U32)index < _imp->tabs.size() ) {
        /*registering this tab for drag&drop*/
        _imp->gui->registerTab(widget,object);

        _imp->tabs.insert(_imp->tabs.begin() + index, std::make_pair(widget,object));
        _imp->modifyingTabBar = true;
        _imp->tabBar->insertTab(index,icon,title);
        _imp->tabBar->updateGeometry(); //< necessary
        _imp->modifyingTabBar = false;
        if (!widget->isVisible()) {
            widget->setVisible(true);
        }
        
        l.unlock();
        _imp->declareTabToPython(widget, object->getScriptName());
        
    } else {
        l.unlock();
        appendTab(widget,object);
    }
    _imp->floatButton->setEnabled(true);
}

void
TabWidget::insertTab(int index,
                     QWidget* widget,
                     ScriptObject* object)
{
    insertTab(index, QIcon(), widget,object);
}

QWidget*
TabWidget::removeTab(int index,bool userAction)
{
    QWidget* tab;
    ScriptObject* obj;
    tabAt(index,&tab,&obj);
    if (!tab || !obj) {
        return 0;
    }
    
    {
        QMutexLocker l(&_imp->tabWidgetStateMutex);
        
        _imp->tabs.erase(_imp->tabs.begin() + index);
        _imp->modifyingTabBar = true;
        _imp->tabBar->removeTab(index);
        _imp->modifyingTabBar = false;
        if (_imp->tabs.size() > 0) {
            l.unlock();
            makeCurrentTab(0);
            l.relock();
        } else {
            _imp->currentWidget = 0;
            _imp->mainLayout->addStretch();
            if ( !_imp->gui->isDraggingPanel() ) {
                l.unlock();
                tryCloseFloatingPane();
                l.relock();
            }
        }
        tab->setParent(_imp->gui);
    }
    ViewerTab* isViewer = dynamic_cast<ViewerTab*>(tab);
    Histogram* isHisto = dynamic_cast<Histogram*>(tab);
    NodeGraph* isGraph = dynamic_cast<NodeGraph*>(tab);
    
    _imp->removeTabToPython(tab, obj->getScriptName());
    
    if (userAction) {
      
        if (isViewer) {
            /*special care is taken if this is a viewer: we also
             need to delete the viewer node.*/
            _imp->gui->removeViewerTab(isViewer,false,false);
        } else if (isHisto) {
            _imp->gui->removeHistogram(isHisto);
        } else {
            ///Do not delete unique widgets such as the properties bin, node graph or curve editor
            tab->setVisible(false);
        }
    } else {
        tab->setVisible(false);
    }
    if (isGraph && _imp->gui->getLastSelectedGraph() == isGraph) {
        _imp->gui->setLastSelectedGraph(0);
    }
	tab->setParent(_imp->gui);

    
    return tab;
}

void
TabWidget::removeTab(QWidget* widget,bool userAction)
{
    int index = -1;

    {
        QMutexLocker l(&_imp->tabWidgetStateMutex);
        for (U32 i = 0; i < _imp->tabs.size(); ++i) {
            if (_imp->tabs[i].first == widget) {
                index = i;
                break;
            }
        }
    }

    if (index != -1) {
        QWidget* tab = removeTab(index,userAction);
        assert(tab == widget);
        (void)tab;
    }
}

void
TabWidget::setCurrentWidget(QWidget* w)
{
    int index = -1;
    {
        QMutexLocker l(&_imp->tabWidgetStateMutex);
        for (U32 i = 0; i < _imp->tabs.size(); ++i) {
            if (_imp->tabs[i].first == w) {
                index = i;
                break;
            }
        }
    }
    if (index != -1) {
        makeCurrentTab(index);
    }
}

void
TabWidget::makeCurrentTab(int index)
{
    if (_imp->modifyingTabBar) {
        return;
    }
    QWidget* tab = tabAt(index);
    if (!tab) {
        return;
    }
    {
        QMutexLocker l(&_imp->tabWidgetStateMutex);
        
        /*Remove previous widget if any*/
        
        if (_imp->currentWidget) {
            QObject::disconnect(_imp->currentWidget, SIGNAL(destroyed()), this, SLOT(onCurrentTabDeleted()));
            _imp->currentWidget->setVisible(false);
            _imp->mainLayout->removeWidget(_imp->currentWidget);
        }

        _imp->mainLayout->addWidget(tab);
        QObject::connect(tab, SIGNAL(destroyed()), this, SLOT(onCurrentTabDeleted()));
        _imp->currentWidget = tab;
    }
    tab->setVisible(true);
    tab->setParent(this);
    _imp->modifyingTabBar = true;
    _imp->tabBar->setCurrentIndex(index);
    _imp->modifyingTabBar = false;
}
                         
void
TabWidget::onCurrentTabDeleted()
{
    QObject* s = sender();
    
    QMutexLocker l(&_imp->tabWidgetStateMutex);

    if (s != _imp->currentWidget) {
        return;
    }
    _imp->currentWidget = 0;
    for (U32 i = 0; i < _imp->tabs.size(); ++i) {
        if (_imp->tabs[i].first == s) {

            _imp->tabs.erase(_imp->tabs.begin() + i);
            _imp->modifyingTabBar = true;
            _imp->tabBar->removeTab(i);
            _imp->modifyingTabBar = false;
            if (_imp->tabs.size() > 0) {
                l.unlock();
                makeCurrentTab(0);
                l.relock();
            } else {
                _imp->currentWidget = 0;
                _imp->mainLayout->addStretch();
                if ( !_imp->gui->isDraggingPanel() ) {
                    l.unlock();
                    tryCloseFloatingPane();
                    l.relock();
                }
            }
            break;
        }
    }
    
}

void
TabWidget::paintEvent(QPaintEvent* e)
{
    QFrame::paintEvent(e);

    if (_imp->drawDropRect) {
        QRect r = rect();
        QPainter p(this);
        QPen pen;
        pen.setBrush( QColor(243,149,0,255) );
        p.setPen(pen);
        //        QPalette* palette = new QPalette();
        //        palette->setColor(QPalette::Foreground,c);
        //        setPalette(*palette);
        r.adjust(0, 0, -1, -1);
        p.drawRect(r);
    }
}

void
TabWidget::dropEvent(QDropEvent* e)
{
    e->accept();
    QString name( e->mimeData()->data("Tab") );
    QWidget* w;
    ScriptObject* obj;
    _imp->gui->findExistingTab(name.toStdString(), &w, &obj);
    if (w && obj) {
        moveTab(w, obj, this);
    }
    _imp->drawDropRect = false;
    setFrameShape(QFrame::NoFrame);
    repaint();
}

TabBar::TabBar(TabWidget* tabWidget,
               QWidget* parent)
    : QTabBar(parent)
      , _dragPos()
      , _dragPix(0)
      , _tabWidget(tabWidget)
      , _processingLeaveEvent(false)
{
    setTabsClosable(true);
    setMouseTracking(true);
    QObject::connect( this, SIGNAL( tabCloseRequested(int) ), tabWidget, SLOT( closeTab(int) ) );
}

void
TabBar::mousePressEvent(QMouseEvent* e)
{
    if ( buttonDownIsLeft(e) ) {
        _dragPos = e->pos();
    }
    QTabBar::mousePressEvent(e);
}

void
TabBar::leaveEvent(QEvent* e)
{
    if (_processingLeaveEvent) {
        QTabBar::leaveEvent(e);
    } else {
        _processingLeaveEvent = true;
        Q_EMIT mouseLeftTabBar();
        QTabBar::leaveEvent(e);
        _processingLeaveEvent = false;
    }
}

/**
 * @brief Given the widget w, tries to find reursively if a parent is a tabwidget and returns it
 **/
static TabWidget* findTabWidgetRecursive(QWidget* w)
{
    assert(w);
    TabWidget* isTab = dynamic_cast<TabWidget*>(w);
    if (isTab) {
        return isTab;
    } else {
        if (w->parentWidget()) {
            return findTabWidgetRecursive(w->parentWidget());
        } else {
            return 0;
        }
    }
    
}

void
TabBar::mouseMoveEvent(QMouseEvent* e)
{
    // If the left button isn't pressed anymore then return
    if ( !buttonDownIsLeft(e) ) {
        return;
    }
    // If the distance is too small then return
    if ( (e->pos() - _dragPos).manhattanLength() < QApplication::startDragDistance() ) {
        return;
    }

    if ( _tabWidget->getGui()->isDraggingPanel() ) {
        const QPoint & globalPos = e->globalPos();
        QWidget* widgetUnderMouse = qApp->widgetAt(globalPos);
        if (widgetUnderMouse) {
            TabWidget* topLvlTabWidget = findTabWidgetRecursive(widgetUnderMouse);
            if (topLvlTabWidget) {
                
                const std::list<TabWidget*> panes = _tabWidget->getGui()->getPanes();
                for (std::list<TabWidget*>::const_iterator it = panes.begin(); it != panes.end(); ++it) {
                    if ( *it == topLvlTabWidget ) {
                        (*it)->setDrawDropRect(true);
                    } else {
                        (*it)->setDrawDropRect(false);
                    }
                }
            }
            
        }
        
        _dragPix->update(globalPos);
        QTabBar::mouseMoveEvent(e);

        return;
    }
    int selectedTabIndex = tabAt( e->pos() );
    if (selectedTabIndex != -1) {
        QPixmap pixmap = makePixmapForDrag(selectedTabIndex);

        _tabWidget->startDragTab(selectedTabIndex);

        _dragPix = new DragPixmap( pixmap,e->pos() );
        _dragPix->update( e->globalPos() );
        _dragPix->show();
        grabMouse();
    }
    QTabBar::mouseMoveEvent(e);
}

QPixmap
TabBar::makePixmapForDrag(int index)
{
    std::vector< std::pair<QString,QIcon > > tabs;

    for (int i = 0; i < count(); ++i) {
        tabs.push_back( std::make_pair( tabText(i),tabIcon(i) ) );
    }

    //remove all tabs
    while (count() > 0) {
        removeTab(0);
    }

    ///insert just the tab we want to screen shot
    addTab(tabs[index].second, tabs[index].first);

    QPixmap currentTabPixmap =  Gui::screenShot( _tabWidget->tabAt(index) );
#if QT_VERSION < 0x050000
    QPixmap tabBarPixmap = QPixmap::grabWidget(this);
#else
    QPixmap tabBarPixmap = grab();
#endif

    ///re-insert all the tabs into the tab bar
    removeTab(0);

    for (U32 i = 0; i < tabs.size(); ++i) {
        addTab(tabs[i].second, tabs[i].first);
    }


    QImage tabBarImg = tabBarPixmap.toImage();
    QImage currentTabImg = currentTabPixmap.toImage();

    //now we just put together the 2 pixmaps and set it with mid transparancy
    QImage ret(currentTabImg.width(),currentTabImg.height() + tabBarImg.height(),QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < tabBarImg.height(); ++y) {
        QRgb* src_pixels = reinterpret_cast<QRgb*>( tabBarImg.scanLine(y) );
        for (int x = 0; x < ret.width(); ++x) {
            if ( x < tabBarImg.width() ) {
                QRgb pix = src_pixels[x];
                ret.setPixel( x, y, qRgba(qRed(pix), qGreen(pix), qBlue(pix), 255) );
            } else {
                ret.setPixel( x, y, qRgba(0, 0, 0, 128) );
            }
        }
    }

    for (int y = 0; y < currentTabImg.height(); ++y) {
        QRgb* src_pixels = reinterpret_cast<QRgb*>( currentTabImg.scanLine(y) );
        for (int x = 0; x < ret.width(); ++x) {
            QRgb pix = src_pixels[x];
            ret.setPixel( x, y + tabBarImg.height(), qRgba(qRed(pix), qGreen(pix), qBlue(pix), 255) );
        }
    }

    return QPixmap::fromImage(ret);
} // makePixmapForDrag

void
TabBar::mouseReleaseEvent(QMouseEvent* e)
{
    bool hasClosedTabWidget = false;
    if ( _tabWidget->getGui()->isDraggingPanel() ) {
        releaseMouse();
        const QPoint & p = e->globalPos();
        hasClosedTabWidget = _tabWidget->stopDragTab(p);
        _dragPix->hide();
        delete _dragPix;
        _dragPix = 0;

        
    }
    if (!hasClosedTabWidget) {
        QTabBar::mouseReleaseEvent(e);
    }
}

bool
TabWidget::stopDragTab(const QPoint & globalPos)
{
    if (count() == 0) {
        tryCloseFloatingPane();
    }

    QWidget* draggedPanel = _imp->gui->stopDragPanel();
    if (!draggedPanel) {
        return false;
    }
    ScriptObject* obj = 0;
    const RegisteredTabs& tabs = _imp->gui->getRegisteredTabs();
    for (RegisteredTabs::const_iterator it = tabs.begin(); it != tabs.end(); ++it) {
        if (it->second.first == draggedPanel) {
            obj = it->second.second;
            break;
        }
        
    }
    if (!obj) {
        return false;
    }
    
    
    const std::list<TabWidget*> panes = _imp->gui->getPanes();
    
    QWidget* widgetUnderMouse = qApp->widgetAt(globalPos);
    bool foundTabWidgetUnderneath = false;
    if (widgetUnderMouse) {
        TabWidget* topLvlTabWidget = findTabWidgetRecursive(widgetUnderMouse);
        if (topLvlTabWidget) {
            
            topLvlTabWidget->appendTab(draggedPanel, obj);
            foundTabWidgetUnderneath = true;
            
        }
    }

    bool ret = false;
    if (!foundTabWidgetUnderneath) {
        ///if we reach here that means the mouse is not over any tab widget, then float the panel
        
        
        
        QPoint windowPos = globalPos;
        FloatingWidget* floatingW = new FloatingWidget(_imp->gui,_imp->gui);
        TabWidget* newTab = new TabWidget(_imp->gui,floatingW);
        _imp->gui->registerPane(newTab);
        newTab->setObjectName_mt_safe( _imp->gui->getAvailablePaneName() );
        newTab->appendTab(draggedPanel,obj);
        floatingW->setWidget(newTab);
        floatingW->move(windowPos);
        _imp->gui->registerFloatingWindow(floatingW);
        
        _imp->gui->checkNumberOfNonFloatingPanes();
        
        bool isClosable = _imp->closeButton->isEnabled();
        if (isClosable && count() == 0) {
            closePane();
            ret = true;
        }
    }
    
    
    for (std::list<TabWidget*>::const_iterator it = panes.begin(); it != panes.end(); ++it) {
        (*it)->setDrawDropRect(false);
    }
    return ret;
}

void
TabWidget::startDragTab(int index)
{
    QWidget* selectedTab = tabAt(index);
    if (!selectedTab) {
        return;
    }

    selectedTab->setParent(this);

    _imp->gui->startDragPanel(selectedTab);

    removeTab(selectedTab, false);
    selectedTab->hide();
}

void
TabWidget::setDrawDropRect(bool draw)
{
    if (draw == _imp->drawDropRect) {
        return;
    }

    _imp->drawDropRect = draw;
    if (draw) {
        setFrameShape(QFrame::Box);
    } else {
        setFrameShape(QFrame::NoFrame);
    }
    repaint();
}

bool
TabWidget::isWithinWidget(const QPoint & globalPos) const
{
    QPoint p = mapToGlobal( QPoint(0,0) );
    QRect bbox( p.x(),p.y(),width(),height() );

    return bbox.contains(globalPos);
}

bool
TabWidget::isFullScreen() const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);

    return _imp->fullScreen;
}

bool
TabWidget::isAnchor() const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);

    return _imp->isAnchor;
}

void
TabWidget::onSetAsAnchorActionTriggered()
{
    const std::list<TabWidget*> & allPanes = _imp->gui->getPanes();

    for (std::list<TabWidget*>::const_iterator it = allPanes.begin(); it != allPanes.end(); ++it) {
        (*it)->setAsAnchor(*it == this);
    }
}

void
TabWidget::onShowHideTabBarActionTriggered()
{
    _imp->tabBarVisible = !_imp->tabBarVisible;
    _imp->header->setVisible(_imp->tabBarVisible);
}

void
TabWidget::setAsAnchor(bool anchor)
{
    {
        QMutexLocker l(&_imp->tabWidgetStateMutex);
        _imp->isAnchor = anchor;
    }
    QPixmap pix;

    if (anchor) {
        appPTR->getIcon(Natron::NATRON_PIXMAP_TAB_WIDGET_LAYOUT_BUTTON_ANCHOR, &pix);
    } else {
        appPTR->getIcon(Natron::NATRON_PIXMAP_TAB_WIDGET_LAYOUT_BUTTON, &pix);
    }
    _imp->leftCornerButton->setIcon( QIcon(pix) );
}

void
TabWidget::keyPressEvent (QKeyEvent* e)
{
    if ( (e->key() == Qt::Key_Space) && modCASIsNone(e) ) {
        bool fullScreen;
        {
            QMutexLocker l(&_imp->tabWidgetStateMutex);
            fullScreen = _imp->fullScreen;
        }
        {
            QMutexLocker l(&_imp->tabWidgetStateMutex);
            _imp->fullScreen = !_imp->fullScreen;
        }
        if (fullScreen) {
            _imp->gui->minimize();
        } else {
            _imp->gui->maximize(this);
        }
        e->accept();
    } else if (isKeybind(kShortcutGroupGlobal, kShortcutIDActionNextTab, e->modifiers(), e->key())) {
        moveToNextTab();
    } else if (isKeybind(kShortcutGroupGlobal, kShortcutIDActionCloseTab, e->modifiers(), e->key())) {
        closeCurrentWidget();
    } else if (isFloatingWindowChild() && isKeybind(kShortcutGroupGlobal, kShortcutIDActionFullscreen, e->modifiers(), e->key())) {
        _imp->gui->toggleFullScreen();
        e->accept();
    } else {
        QFrame::keyPressEvent(e);
    }
}

bool
TabWidget::moveTab(QWidget* what,
                ScriptObject* obj,
                   TabWidget *where)
{
    TabWidget* from = dynamic_cast<TabWidget*>( what->parentWidget() );

    if (from == where) {
        /*We check that even if it is the same TabWidget, it really exists.*/
        bool found = false;
        for (int i = 0; i < from->count(); ++i) {
            if ( what == from->tabAt(i) ) {
                found = true;
                break;
            }
        }
        if (found) {
            return false;
        }
        //it wasn't found somehow
    }

    if (from) {
        from->removeTab(what, false);
    }
    assert(where);
    where->appendTab(what,obj);
    what->setParent(where);
    if ( !where->getGui()->getApp()->getProject()->isLoadingProject() ) {
        where->getGui()->getApp()->triggerAutoSave();
    }

    return true;
}

DragPixmap::DragPixmap(const QPixmap & pixmap,
                       const QPoint & offsetFromMouse)
    : QWidget(0, Qt::ToolTip | Qt::FramelessWindowHint | Qt::X11BypassWindowManagerHint)
      , _pixmap(pixmap)
      , _offset(offsetFromMouse)
{
    setAttribute( Qt::WA_TransparentForMouseEvents );
    resize( _pixmap.width(), _pixmap.height() );
    setWindowOpacity(0.7);
}

void
DragPixmap::update(const QPoint & globalPos)
{
    move(globalPos - _offset);
}

void
DragPixmap::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    p.drawPixmap(0,0,_pixmap);
}

QStringList
TabWidget::getTabScriptNames() const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);
    QStringList ret;

    for (U32 i = 0; i < _imp->tabs.size(); ++i) {
        ret << _imp->tabs[i].second->getScriptName().c_str();
    }

    return ret;
}

QWidget*
TabWidget::currentWidget() const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);
    
    return _imp->currentWidget;
}

void
TabWidget::currentWidget(QWidget** w,ScriptObject** obj) const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);
    *w = _imp->currentWidget;
    for (U32 i = 0; i < _imp->tabs.size(); ++i) {
        if (_imp->tabs[i].first == _imp->currentWidget) {
            *obj = _imp->tabs[i].second;
            return;
        }
    }
    *obj = 0;
}

int
TabWidget::activeIndex() const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);

    for (U32 i = 0; i < _imp->tabs.size(); ++i) {
        if (_imp->tabs[i].first == _imp->currentWidget) {
            return i;
        }
    }

    return -1;
}

void
TabWidget::setObjectName_mt_safe(const QString & str)
{
    std::string oldName = objectName_mt_safe().toStdString();
    {
        QMutexLocker l(&_imp->tabWidgetStateMutex);
        
        setObjectName(str);
    }
    QString tt = Qt::convertFromPlainText(tr(LEFT_HAND_CORNER_BUTTON_TT), Qt::WhiteSpaceNormal) ;
    QString toPre = QString("Script name: <font size = 4><b>%1</font></b><br/>").arg(str);
    tt.prepend(toPre);
    _imp->leftCornerButton->setToolTip(tt);
    
    std::string appID;
    {
        std::stringstream ss;
        ss << "app" << _imp->gui->getApp()->getAppID() + 1;
        appID = ss.str();
    }

    
    std::stringstream ss;
    if (!oldName.empty()) {
        ss << "if hasattr(" << appID << ", '"  << oldName << "'):\n";
        ss << "    del " << appID << "." << oldName << "\n";
    }
    ss << appID << "." << str.toStdString() << " = " << appID << ".getTabWidget('" << str.toStdString() << "')\n";

    std::string script = ss.str();
    std::string err;
    bool ok = Natron::interpretPythonScript(script, &err, 0);
    assert(ok);
}

QString
TabWidget::objectName_mt_safe() const
{
    QMutexLocker l(&_imp->tabWidgetStateMutex);

    return objectName();
}

bool
TabWidget::isFloatingWindowChild() const
{
    QWidget* parent = parentWidget();

    while (parent) {
        FloatingWidget* isFloating = dynamic_cast<FloatingWidget*>(parent);
        if (isFloating) {
            return true;
        }
        parent = parent->parentWidget();
    }

    return false;
}

void
TabWidget::discardGuiPointer()
{
    _imp->gui = 0;
}

void
TabWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (!_imp->tabBarVisible) {
        QSize size = _imp->header->sizeHint();
        if (e->y() <= (size.height() * 1.2)) {
            if (!_imp->header->isVisible()) {
                _imp->header->setVisible(true);
            }
        } else {
            if (_imp->header->isVisible()) {
                _imp->header->setVisible(false);
            }
        }
    }
    if (_imp->gui && _imp->gui->isLeftToolBarDisplayedOnMouseHoverOnly()) {
        _imp->gui->refreshLeftToolBarVisibility(e->globalPos());
    }
    QFrame::mouseMoveEvent(e);
}

void
TabWidget::leaveEvent(QEvent* e)
{
    onTabBarMouseLeft();
    QFrame::leaveEvent(e);
}

void
TabWidget::enterEvent(QEvent* e)
{
    if (_imp->gui) {
        _imp->gui->setLastEnteredTabWidget(this);
    }
    QFrame::leaveEvent(e);
}

void
TabWidget::onTabBarMouseLeft()
{
    if (!_imp->tabBarVisible) {
        if (_imp->header->isVisible()) {
            _imp->header->setVisible(false);
        }
    }
}

void
TabWidget::onTabScriptNameChanged(QWidget* tab,const std::string& oldName,const std::string& newName)
{
    ViewerTab* isViewer = dynamic_cast<ViewerTab*>(tab);
    if (!isViewer) {
        return;
    }
    
    std::string paneName = objectName_mt_safe().toStdString();
    std::string appID = QString("app%1").arg(_imp->gui->getApp()->getAppID() + 1).toStdString();

    std::stringstream ss;
    ss << "if hasattr(" << appID << "." << paneName << ",\"" << oldName << "\"):\n";
    ss << "    del " << appID << "." << paneName << "." << oldName;
    ss << appID << "." << paneName << "." << newName << " = " << appID << ".getViewer(\"" << newName << "\")\n";
    
    std::string err;
    std::string script = ss.str();
    _imp->gui->printAutoDeclaredVariable(script);
    bool ok = Natron::interpretPythonScript(script, &err, 0);
    assert(ok);
}

void
TabWidgetPrivate::declareTabToPython(QWidget* widget,const std::string& tabName)
{
    ViewerTab* isViewer = dynamic_cast<ViewerTab*>(widget);
    PyPanel* isPanel = dynamic_cast<PyPanel*>(widget);
    
    if (!isViewer && !isPanel) {
        return;
    }
    
    std::string paneName = _publicInterface->objectName_mt_safe().toStdString();
    std::string appID = QString("app%1").arg(gui->getApp()->getAppID() + 1).toStdString();
    std::stringstream ss;
    ss << appID << "." << paneName << "." << tabName << " = " << appID << ".";
    if (isViewer) {
        ss << "getViewer('";
    } else {
        ss << "getUserPanel('";
    }
    ss  << tabName << "')\n";
    
    std::string script = ss.str();
    std::string err;
    gui->printAutoDeclaredVariable(script);
    bool ok = Natron::interpretPythonScript(script, &err, 0);
    assert(ok);
}

void
TabWidgetPrivate::removeTabToPython(QWidget* widget,const std::string& tabName)
{
    ViewerTab* isViewer = dynamic_cast<ViewerTab*>(widget);
    PyPanel* isPanel = dynamic_cast<PyPanel*>(widget);
    
    if (!isViewer && !isPanel) {
        return;
    }
    
    std::string paneName = _publicInterface->objectName_mt_safe().toStdString();
    std::string appID = QString("app%1").arg(gui->getApp()->getAppID() + 1).toStdString();
    std::stringstream ss;
    ss << "del " << appID << "." << paneName << "." << tabName ;

    std::string err;
    std::string script = ss.str();
    gui->printAutoDeclaredVariable(script);
    bool ok = Natron::interpretPythonScript(script, &err, 0);
    assert(ok);
}
