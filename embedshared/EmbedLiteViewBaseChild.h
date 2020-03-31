/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZ_VIEW_EMBED_BASE_CHILD_H
#define MOZ_VIEW_EMBED_BASE_CHILD_H

#include "mozilla/embedlite/PEmbedLiteViewChild.h"
#include "mozilla/EventForwards.h"      // for Modifiers

#include "nsIWebBrowser.h"
#include "nsIWidget.h"
#include "nsIWebNavigation.h"
#include "WebBrowserChrome.h"
#include "nsIEmbedBrowserChromeListener.h"
#include "nsIIdleServiceInternal.h"
#include "TabChildHelper.h"
#include "APZCCallbackHelper.h"
#include "EmbedLiteViewChildIface.h"
#include "EmbedLitePuppetWidget.h"

namespace mozilla {

namespace layers {
class APZEventState;
} // namespace layers

namespace embedlite {

class EmbedLiteContentController;
class EmbedLitePuppetWidget;
class EmbedLiteAppThreadChild;

class EmbedLiteViewBaseChild : public PEmbedLiteViewChild,
                               public nsIEmbedBrowserChromeListener,
                               public EmbedLiteViewChildIface,
                               public EmbedLitePuppetWidgetObserver
{
  NS_INLINE_DECL_REFCOUNTING(EmbedLiteViewBaseChild)

  typedef mozilla::layers::APZEventState APZEventState;

public:
  EmbedLiteViewBaseChild(const uint32_t& windowId, const uint32_t& id,
                         const uint32_t& parentId, const bool& isPrivateWindow);

  NS_DECL_NSIEMBEDBROWSERCHROMELISTENER

/*---------TabChildIface---------------*/

  virtual bool
  ZoomToRect(const uint32_t& aPresShellId,
             const ViewID& aViewId,
             const CSSRect& aRect) override;

  virtual bool
  SetTargetAPZC(uint64_t aInputBlockId,
                const nsTArray<ScrollableLayerGuid>& aTargets) override;

  virtual bool
  UpdateZoomConstraints(const uint32_t& aPresShellId,
                        const ViewID& aViewId,
                        const Maybe<ZoomConstraints>& aConstraints) override;

  virtual bool HasMessageListener(const nsAString& aMessageName) override;

  virtual bool DoSendAsyncMessage(const char16_t* aMessageName, const char16_t* aMessage) override;
  virtual bool DoSendSyncMessage(const char16_t* aMessageName,
                                 const char16_t* aMessage,
                                 InfallibleTArray<nsString>* aJSONRetVal) override;
  virtual bool DoCallRpcMessage(const char16_t* aMessageName,
                                const char16_t* aMessage,
                                InfallibleTArray<nsString>* aJSONRetVal) override;

  /**
   * Relay given frame metrics to listeners subscribed via EmbedLiteAppService
   */
  virtual void RelayFrameMetrics(const mozilla::layers::FrameMetrics& aFrameMetrics) override;

  virtual nsIWebNavigation* WebNavigation() override;
  virtual nsIWidget* WebWidget() override;
  virtual bool GetDPI(float* aDPI) override;

/*---------TabChildIface---------------*/

  uint64_t GetOuterID() { return mOuterId; }

  void AddGeckoContentListener(EmbedLiteContentController* listener);
  void RemoveGeckoContentListener(EmbedLiteContentController* listener);

  nsresult GetBrowserChrome(nsIWebBrowserChrome** outChrome);
  nsresult GetBrowser(nsIWebBrowser** outBrowser);
  uint32_t GetID() { return mId; }

  /**
   * This method is used by EmbedLiteAppService::ZoomToRect() only.
   */
  bool GetScrollIdentifiers(uint32_t *aPresShellId, mozilla::layers::FrameMetrics::ViewID *aViewId);

  virtual bool RecvAsyncMessage(const nsString& aMessage, const nsString& aData);

/*---------WidgetIface---------------*/

  virtual void ResetInputState() override;

  virtual bool
  SetInputContext(const int32_t& IMEEnabled,
                  const int32_t& IMEOpen,
                  const nsString& type,
                  const nsString& inputmode,
                  const nsString& actionHint,
                  const int32_t& cause,
                  const int32_t& focusChange) override;

  virtual bool
  GetInputContext(int32_t* IMEEnabled,
                  int32_t* IMEOpen) override;

/*---------WidgetIface---------------*/

  virtual bool ContentReceivedInputBlock(const mozilla::layers::ScrollableLayerGuid& aGuid,
                                         const uint64_t& aInputBlockId,
                                         const bool& aPreventDefault) override;

  virtual bool DoSendContentReceivedInputBlock(const mozilla::layers::ScrollableLayerGuid& aGuid,
                                               uint64_t aInputBlockId,
                                               bool aPreventDefault) override;

  virtual bool DoSendSetAllowedTouchBehavior(uint64_t aInputBlockId,
                                             const nsTArray<mozilla::layers::TouchBehaviorFlags> &aFlags) override;

protected:
  virtual ~EmbedLiteViewBaseChild();

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual bool RecvDestroy() override;
  virtual bool RecvLoadURL(const nsString&) override;
  virtual bool RecvGoBack() override;
  virtual bool RecvGoForward() override;
  virtual bool RecvStopLoad() override;
  virtual bool RecvReload(const bool&) override;

  virtual bool RecvScrollTo(const int &x, const int &y) override;
  virtual bool RecvScrollBy(const int &x, const int &y) override;

  virtual bool RecvSetIsActive(const bool&) override;
  virtual bool RecvSetIsFocused(const bool&) override;
  virtual bool RecvSetThrottlePainting(const bool&) override;
  virtual bool RecvSetMargins(const int&, const int&, const int&, const int&) override;
  virtual bool RecvScheduleUpdate();

  virtual bool RecvSuspendTimeouts() override;
  virtual bool RecvResumeTimeouts() override;
  virtual bool RecvLoadFrameScript(const nsString&) override;
  virtual bool RecvHandleScrollEvent(const bool &isRootScrollFrame, const gfxRect& contentRect,
                                     const gfxSize& scrollSize) override;

  virtual bool RecvUpdateFrame(const mozilla::layers::FrameMetrics& aFrameMetrics) override;
  virtual bool RecvHandleDoubleTap(const CSSPoint&, const Modifiers& aModifiers,
                                   const ScrollableLayerGuid& aGuid) override;
  virtual bool RecvHandleSingleTap(const CSSPoint&, const Modifiers& aModifiers,
                                   const ScrollableLayerGuid& aGuid) override;
  virtual bool RecvHandleLongTap(const CSSPoint& aPoint,
                                 const mozilla::layers::ScrollableLayerGuid& aGuid,
                                 const uint64_t& aInputBlockId) override;
#if 0 // sha1 6afa98a3ea0ba814b77f7aeb162624433e427ccf
  virtual bool RecvAcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId, const uint32_t& aScrollGeneration) override;
#endif
#if 0 // sha1 5753e3da8314bb0522bdbf92819beb6d89faeb06
  virtual bool RecvRequestFlingSnap(const FrameMetrics::ViewID& aScrollId, const CSSPoint& aDestination) override;
#endif
  virtual bool RecvMouseEvent(const nsString& aType,
                              const float& aX,
                              const float& aY,
                              const int32_t& aButton,
                              const int32_t& aClickCount,
                              const int32_t& aModifiers,
                              const bool& aIgnoreRootScrollFrame) override;
  virtual bool RecvHandleTextEvent(const nsString& commit, const nsString& preEdit) override;
  virtual bool RecvHandleKeyPressEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode) override;
  virtual bool RecvHandleKeyReleaseEvent(const int& domKeyCode, const int& gmodifiers, const int& charCode) override;
  virtual bool RecvInputDataTouchEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput&, const uint64_t& aInputBlockId, const nsEventStatus &aApzResponse) override;
  virtual bool RecvInputDataTouchMoveEvent(const ScrollableLayerGuid& aGuid, const mozilla::MultiTouchInput&, const uint64_t& aInputBlockId, const nsEventStatus &aApzResponse) override;

  virtual bool RecvNotifyAPZStateChange(const ViewID& aViewId,
                                        const APZStateChange& aChange,
                                        const int& aArg) override;
  virtual bool RecvNotifyFlushComplete() override;

  virtual bool RecvAddMessageListener(const nsCString&) override;
  virtual bool RecvRemoveMessageListener(const nsCString&) override;
  virtual void RecvAsyncMessage(const nsAString& aMessage, const nsAString& aData) /* FIXME: override */;
  virtual bool RecvAddMessageListeners(InfallibleTArray<nsString>&& messageNames) override;
  virtual bool RecvRemoveMessageListeners(InfallibleTArray<nsString>&& messageNames) override;
  virtual void OnGeckoWindowInitialized() {}

  // Get the pres shell resolution of the document in this tab.
  float GetPresShellResolution() const;

  // EmbedLitePuppetWidgetObserver
  void WidgetBoundsChanged(const nsIntRect&) override;

  // Call this function when the users activity is the direct cause of an
  // event (like a keypress or mouse click).
  void UserActivity();

private:
  friend class TabChildHelper;
  friend class EmbedLiteAppService;
  friend class EmbedLiteAppThreadChild;
  friend class EmbedLiteAppBaseChild;

  void InitGeckoWindow(const uint32_t& parentId, const bool& isPrivateWindow);
  void InitEvent(WidgetGUIEvent& event, nsIntPoint* aPoint = nullptr);

  uint32_t mId;
  uint64_t mOuterId;
  EmbedLiteWindowBaseChild* mWindow; // Not owned
  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<nsIWebBrowser> mWebBrowser;
  nsCOMPtr<nsIIdleServiceInternal> mIdleService;
  RefPtr<WebBrowserChrome> mChrome;
  nsCOMPtr<nsPIDOMWindowOuter> mDOMWindow;
  nsCOMPtr<nsIWebNavigation> mWebNavigation;
  bool mWindowObserverRegistered;
  bool mIsFocused;
  nsIntMargin mMargins;

  RefPtr<TabChildHelper> mHelper;
  bool mIMEComposing;
  uint64_t mPendingTouchPreventedBlockId;

  nsDataHashtable<nsStringHashKey, bool/*start with key*/> mRegisteredMessages;
  nsTArray<EmbedLiteContentController*> mControllerListeners;

  RefPtr<APZEventState> mAPZEventState;
  mozilla::layers::SetAllowedTouchBehaviorCallback mSetAllowedTouchBehaviorCallback;

  DISALLOW_EVIL_CONSTRUCTORS(EmbedLiteViewBaseChild);
};

} // namespace embedlite
} // namespace mozilla

#endif // MOZ_VIEW_EMBED_BASE_CHILD_H
