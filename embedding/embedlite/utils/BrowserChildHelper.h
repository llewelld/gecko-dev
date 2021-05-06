/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __BrowserChildHelper_h_
#define __BrowserChildHelper_h_

#include "nsIObserver.h"
#include "FrameMetrics.h"
#include "nsFrameMessageManager.h"
#include "nsWeakReference.h"
#include "nsIWebNavigation.h"
#include "nsIBrowserChild.h"
#include "nsIWidget.h"
#include "nsIWebBrowserChrome3.h"
#include "InputData.h"
#include "nsDataHashtable.h"
#include "nsIDOMEventListener.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/ContentFrameMessageManager.h"
#include "mozilla/EventDispatcher.h"

class nsPresContext;
class nsIDOMWindowUtils;

namespace mozilla {

namespace layers {
struct ScrollableLayerGuid;
}

namespace embedlite {

class BrowserChildHelperMessageManager : public dom::ContentFrameMessageManager,
                                         public nsIMessageSender,
                                         public dom::DispatcherTrait,
                                         public nsSupportsWeakReference {
 public:
  explicit BrowserChildHelperMessageManager(BrowserChildHelper* aBrowserChild);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(BrowserChildHelperMessageManager,
                                           DOMEventTargetHelper)

  void MarkForCC();

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  virtual dom::Nullable<dom::WindowProxyHolder> GetContent(ErrorResult& aError) override;
  virtual already_AddRefed<nsIDocShell> GetDocShell(
      ErrorResult& aError) override;
  virtual already_AddRefed<nsIEventTarget> GetTabEventTarget() override;
  virtual uint64_t ChromeOuterWindowID() override;

  NS_FORWARD_SAFE_NSIMESSAGESENDER(mMessageManager)

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override {
    aVisitor.mForceContentDispatch = true;
  }

  // Dispatch a runnable related to the global.
  virtual nsresult Dispatch(mozilla::TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) override;

  virtual nsISerialEventTarget* EventTargetFor(
      mozilla::TaskCategory aCategory) const override;

  virtual AbstractThread* AbstractMainThreadFor(
      mozilla::TaskCategory aCategory) override;

  RefPtr<BrowserChildHelper> mBrowserChildHelper;

 protected:
  ~BrowserChildHelperMessageManager();
};

class ContentListener final : public nsIDOMEventListener {
 public:
  explicit ContentListener(BrowserChildHelper* aBrowserChildHelper)
      : mBrowserChildHelper(aBrowserChildHelper) {}
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
 protected:
  ~ContentListener() = default;
  BrowserChildHelper* mBrowserChildHelper;
};

class EmbedLiteViewChildIface;
class BrowserChildHelper : public dom::ipc::MessageManagerCallback,
                           public nsMessageManagerScriptExecutor,
                           public nsIDOMEventListener,
                           public nsSupportsWeakReference,
                           public nsIBrowserChild,
                           public nsIObserver
{
public:
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  BrowserChildHelper(EmbedLiteViewChildIface* aView);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER
  NS_DECL_NSIBROWSERCHILD
  NS_DECL_NSIOBSERVER

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(BrowserChildHelper,
                                                         nsIBrowserChild)

  bool UpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);

  nsIWebNavigation* WebNavigation() const;
  nsIWidget* WebWidget();

  bool DoLoadMessageManagerScript(const nsAString& aURL, bool aRunInGlobalScope);
  bool DoSendBlockingMessage(JSContext* aCx,
                             const nsAString& aMessage,
                             mozilla::dom::ipc::StructuredCloneData& aData,
                             JS::Handle<JSObject *> aCpows,
                             nsIPrincipal* aPrincipal,
                             nsTArray<mozilla::dom::ipc::StructuredCloneData>* aRetVal,
                             bool aIsSync);
  nsresult DoSendAsyncMessage(JSContext* aCx,
                              const nsAString& aMessage,
                              mozilla::dom::ipc::StructuredCloneData& aData,
                              JS::Handle<JSObject *> aCpows,
                              nsIPrincipal* aPrincipal);
  bool DoUpdateZoomConstraints(const uint32_t& aPresShellId,
                               const mozilla::layers::FrameMetrics::ViewID& aViewId,
                               const Maybe<mozilla::layers::ZoomConstraints>& aConstraints);
  ScreenIntSize GetInnerSize();

  void ProcessUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics);

  bool UpdateFrameHandler(const mozilla::layers::FrameMetrics& aFrameMetrics);

  void ReportSizeUpdate(const LayoutDeviceIntRect& aRect);

  mozilla::CSSPoint ApplyPointTransform(const LayoutDevicePoint& aPoint,
                                        const mozilla::layers::ScrollableLayerGuid& aGuid,
                                        bool *ok);

  void OpenIPC() { mIPCOpen = true; }

  uint64_t ChromeOuterWindowID() const;

protected:
  virtual ~BrowserChildHelper();
  nsIWidget* GetWidget(nsPoint* aOffset);
  nsPresContext* GetPresContext();
  // Sends a simulated mouse event from a touch event for compatibility.
  bool ConvertMutiTouchInputToEvent(const mozilla::MultiTouchInput& aData,
                                    WidgetTouchEvent& aEvent);
  bool HasValidInnerSize();

  nsCOMPtr<nsIWebBrowserChrome3> mWebBrowserChrome;
  RefPtr<BrowserChildHelperMessageManager> mBrowserChildMessageManager;

private:
  bool InitBrowserChildHelperMessageManager();
  void Disconnect();
  void Unload();
  bool IPCOpen() const { return mIPCOpen; }

  // Get the Document for the top-level window in this tab.
  already_AddRefed<dom::Document> GetTopLevelDocument() const;

  // Get the pres-shell of the document for the top-level window in this tab.
  nsIPresShell* GetTopLevelPresShell() const;

  // XXX/bug 780335: Do the work the browser chrome script does in C++ instead
  // so we don't need things like this.
  void DispatchMessageManagerMessage(const nsAString& aMessageName,
                                     const nsAString& aJSONData);

  friend class EmbedLiteViewThreadChild;
  friend class EmbedLiteViewProcessChild;
  friend class EmbedLiteViewChildIface;
  friend class EmbedLiteViewChild;
  EmbedLiteViewChildIface* mView;
  bool mHasValidInnerSize;
  bool mIPCOpen;
  ScreenIntSize mInnerSize;
  bool mShouldSendWebProgressEventsToParent;
  // Whether or not this tab has siblings (other tabs in the same window).
  // This is one factor used when choosing to allow or deny a non-system
  // script's attempt to resize the window.
  bool mHasSiblings;
};

}
}

#endif

