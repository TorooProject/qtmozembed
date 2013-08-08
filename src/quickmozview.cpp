/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "quickmozview.h"

#include "mozilla-config.h"
#include "qmozcontext.h"
#include "qmozembedlog.h"
#include "InputData.h"
#include "mozilla/embedlite/EmbedLiteView.h"
#include "mozilla/embedlite/EmbedLiteApp.h"

#include <QTimer>
#include <QThread>
#include <QtOpenGL/QGLContext>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QtQuick/qquickwindow.h>
#include <QtGui/QOpenGLShaderProgram>
#include <QtGui/QOpenGLContext>
#include <QSGSimpleRectNode>
#include <QSGSimpleTextureNode>
#include "qgraphicsmozview_p.h"
#include "EmbedQtKeyUtils.h"
#include "qmozviewsgnode.h"
#include "qsgthreadobject.h"
#include "qmcthreadobject.h"
#include "assert.h"

using namespace mozilla;
using namespace mozilla::embedlite;

QuickMozView::QuickMozView(QQuickItem *parent)
  : QQuickItem(parent)
  , d(new QGraphicsMozViewPrivate(new IMozQView<QuickMozView>(*this)))
  , mParentID(0)
  , mUseQmlMouse(false)
{
    static bool Initialized = false;
    if (!Initialized) {
        qmlRegisterType<QMozReturnValue>("QtMozilla", 1, 0, "QMozReturnValue");
        Initialized = true;
    }

    setFlag(ItemHasContents, true);

    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton | Qt::MiddleButton);
    setFlag(ItemClipsChildrenToShape, true);
    setFlag(ItemIsFocusScope, true);
    setFlag(ItemAcceptsDrops, true);
    setFlag(ItemAcceptsInputMethod, true);

    d->mContext = QMozContext::GetInstance();
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
//    connect(this, SIGNAL(requestGLContext(bool&,QSize&)), this, SLOT(onRequestGLContext(bool&,QSize&)));
    connect(this, SIGNAL(setIsActive(bool)), this, SLOT(onSetIsActive(bool)));
    connect(this, SIGNAL(updateThreaded()), this, SLOT(update()));
    if (!d->mContext->initialized()) {
        connect(d->mContext, SIGNAL(onInitialized()), this, SLOT(onInitialized()));
    } else {
        QTimer::singleShot(0, this, SLOT(onInitialized()));
    }
}

QuickMozView::~QuickMozView()
{
    if (d->mView) {
        d->mView->SetListener(NULL);
        d->mContext->GetApp()->DestroyView(d->mView);
    }
    delete d;
}

void
QuickMozView::SetIsActive(bool aIsActive)
{
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
    Q_EMIT setIsActive(aIsActive);
}

void
QuickMozView::onSetIsActive(bool aIsActive)
{
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
    d->mView->SetIsActive(aIsActive);
}

void
QuickMozView::onInitialized()
{
    LOGT("QuickMozView");
    if (!d->mView) {
      const QOpenGLContext* ctx = QOpenGLContext::currentContext();
//      d->mContext->GetApp()->SetIsAccelerated(ctx && ctx->surface() && !getenv("SWRENDER"));
      d->mContext->GetApp()->SetIsAccelerated(true);
      d->mView = d->mContext->GetApp()->CreateView();
      d->mView->SetListener(d);
    }
}

void QuickMozView::requestGLContext(bool& hasContext, QSize& viewPortSize)
{
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p, sgRenderThread:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), mSGRenderer->thread());
    hasContext = d->mHasContext;
    viewPortSize = d->mGLSurfaceSize;
    if (mSGRenderer->thread() == QThread::currentThread()) {
        mSGRenderer->makeContextCurrent();
    }
    if (mMCRenderer->thread() == QThread::currentThread()) {
        mMCRenderer->makeContextCurrent();
    }
}

void QuickMozView::RenderToCurrentContext(QMatrix affine, QSize size)
{
    printf(">>>>>>Func:%s::%d START RENDER curThread:%p, curThrId:%p, sgRenderThread:%p, mcThread:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), mSGRenderer->thread(), mMCRenderer->thread());
    if (mSGRenderer->thread() == QThread::currentThread()) {
//        Q_EMIT renderRequest(affine, size);
        printf(">>>>>>Func:%s::%d PUSH SUBTHREAD START RENDER curThread:%p, curThrId:%p, sgRenderThread:%p, mcThread:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), mSGRenderer->thread(), mMCRenderer->thread());
//        mSGRenderer->RenderToCurrentContext(affine,size);
        mMCRenderer->ProcessInGeckoThread(affine, size);
        //bool ret = mMCRenderer->thread()->wait();
        printf(">>>>>>Func:%s::%d PUSH SUBTHREAD END RENDER curThread:%p, curThrId:%p, sgRenderThread:%p, mcThread:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), mSGRenderer->thread(), mMCRenderer->thread());
//        mSGRenderer->RenderToCurrentContext(affine,size);
    }
    printf(">>>>>>Func:%s::%d END RENDER  curThread:%p, curThrId:%p, sgRenderThread:%p, mcThread:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), mSGRenderer->thread(), mMCRenderer->thread());
}

void QuickMozView::RenderToCurrentContext2(QMatrix affine, QSize size)
{
    printf(">>>>>>Func:%s::%d START RENDER curThread:%p, curThrId:%p, sgRenderThread:%p, mcThread:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), mSGRenderer->thread(), mMCRenderer->thread());
    gfxMatrix matr(affine.m11(), affine.m12(), affine.m21(), affine.m22(), affine.dx(), affine.dy());
    d->mView->SetGLViewTransform(matr);
    d->mView->SetViewClipping(0, 0, size.width(), size.height());
    d->mView->RenderGL();
    printf(">>>>>>Func:%s::%d END RENDER  curThread:%p, curThrId:%p, sgRenderThread:%p, mcThread:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), mSGRenderer->thread(), mMCRenderer->thread());
}


void QuickMozView::createGeckoGLContext()
{
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p, sgRenderThread:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), mSGRenderer->thread());
    if (!mMCRenderer) {
        mMCRenderer = new QMCThreadObject(this, mSGRenderer);
//        connect(this, SIGNAL(renderRequest(QMatrix,QSize)), mMCRenderer, SLOT(onRenderRequested(QMatrix,QSize)), Qt::BlockingQueuedConnection);
//        connect(this, SIGNAL(renderRequest(QMatrix,QSize)), mMCRenderer, SLOT(onRenderRequested(QMatrix,QSize)));
    }
}

void
QuickMozView::onRequestGLContext(bool& hasContext, QSize& viewPortSize)
{
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
    hasContext = false;
    if (d->mContext->GetApp()->IsAccelerated() && d->mHasContext) {
        QRectF r(0, 0, d->mGLSurfaceSize.width(), d->mGLSurfaceSize.height());
        r = mapRectToScene(r);
        viewPortSize = r.size().toSize();
        printf(">>>>>>Func:%s::%d YO HAS CONTEXT !!! curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
        hasContext = true;
    }
}

void QuickMozView::updateGLContextInfo(bool hasContext, QSize viewPortSize)
{
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p, hasContext:%i, size[%i,%i]\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId(), hasContext, viewPortSize.width(), viewPortSize.height());
    d->mHasContext = hasContext;
    d->mGLSurfaceSize = viewPortSize;
    QRectF r(0, 0, d->mGLSurfaceSize.width(), d->mGLSurfaceSize.height());
    r = mapRectToScene(r);
    d->mGLSurfaceSize = r.size().toSize();
}

void
QuickMozView::onSheduleUpdate()
{
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
    update();
}

void QuickMozView::itemChange(ItemChange change, const ItemChangeData &)
{
    if (change == ItemSceneChange) {
        QQuickWindow *win = window();
        if (!win)
            return;
        connect(win, SIGNAL(beforeRendering()), this, SLOT(beforeRendering()), Qt::DirectConnection);
        connect(win, SIGNAL(sceneGraphInitialized()), this, SLOT(sceneGraphInitialized()), Qt::DirectConnection);
        win->setClearBeforeRendering(false);
    }
}

void QuickMozView::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    d->mSize = newGeometry.size().toSize();
    d->UpdateViewSize();
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

void QuickMozView::sceneGraphInitialized()
{
}

void QuickMozView::beforeRendering()
{
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
    if (!mSGRenderer) {
        mSGRenderer = new QSGThreadObject(this);
        connect(mSGRenderer, SIGNAL(updateGLContextInfo(bool,QSize)), this, SLOT(updateGLContextInfo(bool,QSize)));
        connect(mSGRenderer, SIGNAL(updateSignal()), this, SLOT(onSheduleUpdate()));
        mSGRenderer->setupCurrentGLContext();
//        mMCThread = new QMCThread(this, mSGRenderer);
//        mMCRenderer = mMCThread->mMCRenderer;
    }
    if (mSGRenderer) {
        mSGRenderer->setupCurrentGLContext();
    }
    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
    if (!d->mGraphicsViewAssigned) {
        d->UpdateViewSize();
        d->mGraphicsViewAssigned = true;
        // Disable for future gl context in case if we did not get it yet
        if (d->mViewInitialized &&
            d->mContext->GetApp()->IsAccelerated() &&
            !QOpenGLContext::currentContext()) {
            LOGT("Gecko is setup for GL rendering but no context available on paint, disable it");
            d->mContext->setIsAccelerated(true);
        }
    }
}

QSGNode*
QuickMozView::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data)
{
    if (!d->mViewInitialized)
        return oldNode;


    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
    if (!d->mContext->GetApp()->IsAccelerated()) {
        printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p NOT ACCEL\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
        QSGSimpleTextureNode *n = static_cast<QSGSimpleTextureNode*>(oldNode);
        if (!n) {
            n = new QSGSimpleTextureNode();
        }
        QRect r(boundingRect().toRect());
        if (d->mTempBufferImage.isNull() || d->mTempBufferImage.width() != r.width() || d->mTempBufferImage.height() != r.height()) {
            d->mTempBufferImage = QImage(r.size(), QImage::Format_RGB32);
        }
        d->mView->RenderToImage(d->mTempBufferImage.bits(), d->mTempBufferImage.width(),
                                d->mTempBufferImage.height(), d->mTempBufferImage.bytesPerLine(),
                                d->mTempBufferImage.depth());

        if (d->mTempTexture)
            delete d->mTempTexture;
        d->mTempTexture = window()->createTextureFromImage(d->mTempBufferImage);
        n->setTexture(d->mTempTexture);
        n->setRect(boundingRect());
        return n;
    }

    printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p ACCEL!!!\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
    QMozViewSGNode* node = static_cast<QMozViewSGNode*>(oldNode);

    const QWindow* window = this->window();
    assert(window);

    if (!node)
        node = new QMozViewSGNode;

    node->setRenderer(d, this);
    node->markDirty(QSGNode::DirtyMaterial);
    update();

    return node;
}

void QuickMozView::cleanup()
{
}

void QuickMozView::mouseMoveEvent(QMouseEvent* e)
{
    if (!mUseQmlMouse) {
        const bool accepted = e->isAccepted();
        recvMouseMove(e->pos().x(), e->pos().y());
        e->setAccepted(accepted);
    }
    else {
        QQuickItem::mouseMoveEvent(e);
    }
}

void QuickMozView::mousePressEvent(QMouseEvent* e)
{
    if (!mUseQmlMouse) {
        const bool accepted = e->isAccepted();
        recvMousePress(e->pos().x(), e->pos().y());
        e->setAccepted(accepted);
    }
    else {
        QQuickItem::mousePressEvent(e);
    }
}

void QuickMozView::mouseReleaseEvent(QMouseEvent* e)
{
    if (!mUseQmlMouse) {
        const bool accepted = e->isAccepted();
        recvMouseRelease(e->pos().x(), e->pos().y());
        e->setAccepted(accepted);
    }
    else {
        QQuickItem::mouseReleaseEvent(e);
    }
}

void QuickMozView::setInputMethodHints(Qt::InputMethodHints hints)
{
    d->mInputMethodHints = hints;
}

void QuickMozView::inputMethodEvent(QInputMethodEvent* event)
{
    LOGT("cStr:%s, preStr:%s, replLen:%i, replSt:%i", event->commitString().toUtf8().data(), event->preeditString().toUtf8().data(), event->replacementLength(), event->replacementStart());
    if (d->mViewInitialized) {
        d->mView->SendTextEvent(event->commitString().toUtf8().data(), event->preeditString().toUtf8().data());
    }
}

void QuickMozView::keyPressEvent(QKeyEvent* event)
{
    if (!d->mViewInitialized)
        return;

    int32_t gmodifiers = MozKey::QtModifierToDOMModifier(event->modifiers());
    int32_t domKeyCode = MozKey::QtKeyCodeToDOMKeyCode(event->key(), event->modifiers());
    int32_t charCode = 0;
    if (event->text().length() && event->text()[0].isPrint()) {
        charCode = (int32_t)event->text()[0].unicode();
        if (getenv("USE_TEXT_EVENTS")) {
            return;
        }
    }
    d->mView->SendKeyPress(domKeyCode, gmodifiers, charCode);
}

void QuickMozView::keyReleaseEvent(QKeyEvent* event)
{
    if (!d->mViewInitialized)
        return;

    int32_t gmodifiers = MozKey::QtModifierToDOMModifier(event->modifiers());
    int32_t domKeyCode = MozKey::QtKeyCodeToDOMKeyCode(event->key(), event->modifiers());
    int32_t charCode = 0;
    if (event->text().length() && event->text()[0].isPrint()) {
        charCode = (int32_t)event->text()[0].unicode();
        if (getenv("USE_TEXT_EVENTS")) {
            d->mView->SendTextEvent(event->text().toUtf8().data(), "");
            return;
        }
    }
    d->mView->SendKeyRelease(domKeyCode, gmodifiers, charCode);
}

QVariant
QuickMozView::inputMethodQuery(Qt::InputMethodQuery property) const
{
    switch (property) {
    case Qt::ImEnabled:
        return QVariant((bool) d->mIsInputFieldFocused);
    case Qt::ImHints:
        return QVariant((int) d->mInputMethodHints);
    default:
        return QVariant();
    }
}

void QuickMozView::focusInEvent(QFocusEvent* event)
{
    d->SetIsFocused(true);
    QQuickItem::focusInEvent(event);
}

void QuickMozView::focusOutEvent(QFocusEvent* event)
{
    d->SetIsFocused(false);
    QQuickItem::focusOutEvent(event);
}

void QuickMozView::forceViewActiveFocus()
{
    forceActiveFocus();
    if (d->mViewInitialized) {
        d->mView->SetIsActive(true);
        d->mView->SetIsFocused(true);
    }
}

void QuickMozView::Invalidate()
{
    if (QThread::currentThread() != thread()) {
        printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
        Q_EMIT updateThreaded();
    } else {
        printf(">>>>>>Func:%s::%d curThread:%p, curThrId:%p\n", __PRETTY_FUNCTION__, __LINE__, QThread::currentThread(), (void*)QThread::currentThreadId());
        update();
    }
}

QUrl QuickMozView::url() const
{
    return QUrl(d->mLocation);
}

void QuickMozView::setUrl(const QUrl& url)
{
    load(url.toString());
}

QString QuickMozView::title() const
{
    return d->mTitle;
}

int QuickMozView::loadProgress() const
{
    return d->mProgress;
}

bool QuickMozView::canGoBack() const
{
    return d->mCanGoBack;
}

bool QuickMozView::canGoForward() const
{
    return d->mCanGoForward;
}

bool QuickMozView::loading() const
{
    return d->mIsLoading;
}

QRect QuickMozView::contentRect() const
{
    return d->mContentRect;
}

QSize QuickMozView::scrollableSize() const
{
    return d->mScrollableSize;
}

QPointF QuickMozView::scrollableOffset() const
{
    return d->mScrollableOffset;
}

float QuickMozView::resolution() const
{
    return d->mContentResolution;
}

bool QuickMozView::isPainted() const
{
    return d->mIsPainted;
}

QColor QuickMozView::bgcolor() const
{
    return d->mBgColor;
}

bool QuickMozView::getUseQmlMouse()
{
    return mUseQmlMouse;
}

void QuickMozView::setUseQmlMouse(bool value)
{
    mUseQmlMouse = value;
}

void QuickMozView::loadHtml(const QString& html, const QUrl& baseUrl)
{
    LOGT();
}

void QuickMozView::goBack()
{
    if (!d->mViewInitialized)
        return;
    d->mView->GoBack();
}

void QuickMozView::goForward()
{
    if (!d->mViewInitialized)
        return;
    d->mView->GoForward();
}

void QuickMozView::stop()
{
    if (!d->mViewInitialized)
        return;
    d->mView->StopLoad();
}

void QuickMozView::reload()
{
    if (!d->mViewInitialized)
        return;
    d->mView->Reload(false);
}

void QuickMozView::load(const QString& url)
{
    if (url.isEmpty())
        return;

    if (!d->mViewInitialized) {
        return;
    }
    LOGT("url: %s", url.toUtf8().data());
    d->mProgress = 0;
    d->mView->LoadURL(url.toUtf8().data());
}

void QuickMozView::sendAsyncMessage(const QString& name, const QVariant& variant)
{
    if (!d->mViewInitialized)
        return;

    QJsonDocument doc = QJsonDocument::fromVariant(variant);
    QByteArray array = doc.toJson();

    d->mView->SendAsyncMessage((const PRUnichar*)name.constData(), NS_ConvertUTF8toUTF16(array.constData()).get());
}

void QuickMozView::addMessageListener(const QString& name)
{
    if (!d->mViewInitialized)
        return;

    d->mView->AddMessageListener(name.toUtf8().data());
}

void QuickMozView::addMessageListeners(const QStringList& messageNamesList)
{
    if (!d->mViewInitialized)
        return;

    nsTArray<nsString> messages;
    for (int i = 0; i < messageNamesList.size(); i++) {
        messages.AppendElement((PRUnichar*)messageNamesList.at(i).data());
    }
    d->mView->AddMessageListeners(messages);
}

void QuickMozView::loadFrameScript(const QString& name)
{
    d->mView->LoadFrameScript(name.toUtf8().data());
}

void QuickMozView::newWindow(const QString& url)
{
    LOGT("New Window: %s", url.toUtf8().data());
}

quint32 QuickMozView::uniqueID() const
{
    return d->mView ? d->mView->GetUniqueID() : 0;
}

void QuickMozView::setParentID(unsigned aParentID)
{
    mParentID = aParentID;
    if (mParentID) {
        onInitialized();
    }
}

void QuickMozView::synthTouchBegin(const QVariant& touches)
{
    QList<QVariant> list = touches.toList();
    d->mTouchTime.restart();
    MultiTouchInput meventStart(MultiTouchInput::MULTITOUCH_START, d->mTouchTime.elapsed());
    int ptId = 0;
    for(QList<QVariant>::iterator it = list.begin(); it != list.end(); it++)
    {
        const QPointF pt = (*it).toPointF();
        mozilla::ScreenIntPoint nspt(pt.x(), pt.y());
        ptId++;
        meventStart.mTouches.AppendElement(SingleTouchData(ptId,
                                                           nspt,
                                                           mozilla::ScreenSize(1, 1),
                                                           180.0f,
                                                           1.0f));
    }
    d->mView->ReceiveInputEvent(meventStart);
}

void QuickMozView::synthTouchMove(const QVariant& touches)
{
    QList<QVariant> list = touches.toList();
    MultiTouchInput meventStart(MultiTouchInput::MULTITOUCH_MOVE, d->mTouchTime.elapsed());
    int ptId = 0;
    for(QList<QVariant>::iterator it = list.begin(); it != list.end(); it++)
    {
        const QPointF pt = (*it).toPointF();
        mozilla::ScreenIntPoint nspt(pt.x(), pt.y());
        ptId++;
        meventStart.mTouches.AppendElement(SingleTouchData(ptId,
                                                           nspt,
                                                           mozilla::ScreenSize(1, 1),
                                                           180.0f,
                                                           1.0f));
    }
    d->mView->ReceiveInputEvent(meventStart);
}

void QuickMozView::synthTouchEnd(const QVariant& touches)
{
    QList<QVariant> list = touches.toList();
    MultiTouchInput meventStart(MultiTouchInput::MULTITOUCH_END, d->mTouchTime.elapsed());
    int ptId = 0;
    for(QList<QVariant>::iterator it = list.begin(); it != list.end(); it++)
    {
        const QPointF pt = (*it).toPointF();
        mozilla::ScreenIntPoint nspt(pt.x(), pt.y());
        ptId++;
        meventStart.mTouches.AppendElement(SingleTouchData(ptId,
                                                           nspt,
                                                           mozilla::ScreenSize(1, 1),
                                                           180.0f,
                                                           1.0f));
    }
    d->mView->ReceiveInputEvent(meventStart);
}

void QuickMozView::scrollTo(const QPointF& position)
{
    LOGT("NOT IMPLEMENTED");
}

void QuickMozView::suspendView()
{
    if (!d->mView) {
        return;
    }
    d->mView->SetIsActive(false);
    d->mView->SuspendTimeouts();
}

void QuickMozView::resumeView()
{
    if (!d->mView) {
        return;
    }
    d->mView->SetIsActive(true);
    d->mView->ResumeTimeouts();
}

void QuickMozView::recvMouseMove(int posX, int posY)
{
    if (d->mViewInitialized && !d->mPendingTouchEvent) {
        MultiTouchInput event(MultiTouchInput::MULTITOUCH_MOVE, d->mPanningTime.elapsed());
        event.mTouches.AppendElement(SingleTouchData(0,
                                     mozilla::ScreenIntPoint(posX, posY),
                                     mozilla::ScreenSize(1, 1),
                                     180.0f,
                                     1.0f));
        d->ReceiveInputEvent(event);
    }
}

void QuickMozView::recvMousePress(int posX, int posY)
{
    d->mPanningTime.restart();
    forceViewActiveFocus();
    if (d->mViewInitialized && !d->mPendingTouchEvent) {
        MultiTouchInput event(MultiTouchInput::MULTITOUCH_START, d->mPanningTime.elapsed());
        event.mTouches.AppendElement(SingleTouchData(0,
                                     mozilla::ScreenIntPoint(posX, posY),
                                     mozilla::ScreenSize(1, 1),
                                     180.0f,
                                     1.0f));
        d->ReceiveInputEvent(event);
    }
}

void QuickMozView::recvMouseRelease(int posX, int posY)
{
    if (d->mViewInitialized && !d->mPendingTouchEvent) {
        MultiTouchInput event(MultiTouchInput::MULTITOUCH_END, d->mPanningTime.elapsed());
        event.mTouches.AppendElement(SingleTouchData(0,
                                     mozilla::ScreenIntPoint(posX, posY),
                                     mozilla::ScreenSize(1, 1),
                                     180.0f,
                                     1.0f));
        d->ReceiveInputEvent(event);
    }
    if (d->mPendingTouchEvent) {
        d->mPendingTouchEvent = false;
    }
}
