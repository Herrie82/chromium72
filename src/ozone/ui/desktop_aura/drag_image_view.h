// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OZONE_UI_DESKTOP_AURA_DRAG_IMAGE_VIEW_H_
#define OZONE_UI_DESKTOP_AURA_DRAG_IMAGE_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/views_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Image;
}

namespace views {
class Widget;

// This class allows to show a (native) view always on top of everything. It
// does this by creating a widget and setting the content as the given view. The
// caller can then use this object to freely move / drag it around on the
// desktop in screen coordinates.
class VIEWS_EXPORT DragImageView : public views::ImageView {
 public:
  // |root_window| is the root window on which to create the drag image widget.
  // |source| is the event source that started this drag drop operation (touch
  // or mouse). It is used to determine attributes of the drag image such as
  // whether to show drag operation hint on top of the image.
  DragImageView(aura::Window* root_window,
                ui::DragDropTypes::DragEventSource source);
  ~DragImageView() override;

  // Sets the bounds of the native widget in screen
  // coordinates.
  // TODO(oshima): Looks like this is root window's
  // coordinate. Change this to screen's coordinate.
  void SetBoundsInScreen(const gfx::Rect& bounds);

  // Sets the position of the native widget.
  void SetScreenPosition(const gfx::Point& position);

  // Gets the image position in screen coordinates.
  gfx::Rect GetBoundsInScreen() const;

  // Sets the visibility of the native widget.
  void SetWidgetVisible(bool visible);

  // For touch drag drop, we show a hint corresponding to the drag operation
  // (since no mouse cursor is visible). These functions set the hint
  // parameters.
  void SetTouchDragOperationHintOff();

  // |operation| is a bit field indicating allowable drag operations from
  // ui::DragDropTypes::DragOperation.
  void SetTouchDragOperation(int operation);
  void SetTouchDragOperationHintPosition(const gfx::Point& position);

  // Sets the |opacity| of the image view between 0.0 and 1.0.
  void SetOpacity(float opacity);

  gfx::Size GetMinimumSize() const override;

 private:
  gfx::Image* DragHint() const;
  // Drag hint images are only drawn when the input source is touch.
  bool ShouldDrawDragHint() const;

  // Overridden from views::ImageView.
  void OnPaint(gfx::Canvas* canvas) override;

  // Overridden from views::view
  void Layout() override;

  std::unique_ptr<views::Widget> widget_;

  // Save the requested drag image size. We may need to display a drag hint
  // image, which potentially expands |widget_|'s size. That drag hint image
  // may be disabled (e.g. during the drag cancel animation). In that case,
  // we need to know the originally requested size to render the drag image.
  gfx::Size drag_image_size_;

  ui::DragDropTypes::DragEventSource drag_event_source_;

  // Bitmask of ui::DragDropTypes::DragOperation values.
  int touch_drag_operation_;
  gfx::Point touch_drag_operation_indicator_position_;

  DISALLOW_COPY_AND_ASSIGN(DragImageView);
};

}  // namespace views

#endif  // OZONE_UI_DESKTOP_AURA_DRAG_IMAGE_VIEW_H_
