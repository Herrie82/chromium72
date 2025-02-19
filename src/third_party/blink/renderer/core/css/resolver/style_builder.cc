/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/css/css_animations.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/properties/longhands/variable.h"
#include "third_party/blink/renderer/core/css/resolver/css_variable_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

static inline bool IsCSSNavigationProperty(CSSPropertyID id) {
  switch (id) {
    case CSSPropertyNavIndex:
    case CSSPropertyNavLeft:
    case CSSPropertyNavRight:
    case CSSPropertyNavUp:
    case CSSPropertyNavDown:
      return true;
    default:
      return false;
  }
}

void ApplyCSSNavigationProperty(CSSPropertyID id,
                                StyleResolverState& state,
                                const CSSValue& value,
                                bool is_initial,
                                bool is_inherit) {
  if (is_inherit) {
    state.Style()->InheritNavigation(id, state.ParentStyle());
    return;
  }

  if (id == CSSPropertyNavIndex) {
    bool is_auto = (value.IsIdentifierValue() &&
                    ToCSSIdentifierValue(value).GetValueID() == CSSValueAuto);
    is_auto = (is_initial || is_auto);

    scoped_refptr<StyleNavigationIndex> navigation_index =
        state.Style()->AccessNavigationIndex();
    DCHECK(navigation_index.get());

    if (is_auto) {
      navigation_index->is_auto = true;
      return;
    }

    navigation_index->is_auto = false;
    navigation_index->index =
        std::max(static_cast<int>(std::numeric_limits<short>::min()),
                 std::min(ToCSSPrimitiveValue(value).GetIntValue(),
                          static_cast<int>(std::numeric_limits<short>::max())));

    state.GetElement()->setTabIndex(navigation_index->index);
  } else if (id == CSSPropertyNavLeft || id == CSSPropertyNavRight ||
             id == CSSPropertyNavUp || id == CSSPropertyNavDown) {
    scoped_refptr<StyleNavigationData> navigation =
        state.Style()->AccessNavigation(id);
    DCHECK(navigation.get());

    if (value.IsIdentifierValue() &&
        ToCSSIdentifierValue(value).GetValueID() == CSSValueAuto) {
      navigation->flag = StyleNavigationData::NAVIGATION_TARGET_NONE;
      return;
    }

    if (value.IsCustomIdentValue()) {
      navigation->flag = StyleNavigationData::NAVIGATION_TARGET_CURRENT;
      navigation->id = ToCSSCustomIdentValue(value).Value();
    } else if (value.IsValueList()) {
      const CSSValueList& list = ToCSSValueList(value);
      DCHECK_EQ(list.length(), 2U);
      const CSSValue& item0 = list.Item(0);
      if (!item0.IsCustomIdentValue())
        return;
      navigation->id = ToCSSCustomIdentValue(item0).Value();

      const CSSValue& item1 = list.Item(1);
      if (item1.IsPrimitiveValue() &&
          ToCSSIdentifierValue(value).GetValueID() == CSSValueRoot) {
        navigation->flag = StyleNavigationData::NAVIGATION_TARGET_ROOT;
      } else if (item1.IsCustomIdentValue()) {
        navigation->flag = StyleNavigationData::NAVIGATION_TARGET_NAME;
        navigation->target = ToCSSCustomIdentValue(item1).Value();
      } else {
        navigation->flag = StyleNavigationData::NAVIGATION_TARGET_CURRENT;
      }
    }
  } else {
    NOTREACHED();
  }
}

}  // namespace

void StyleBuilder::ApplyProperty(const CSSPropertyName& name,
                                 StyleResolverState& state,
                                 const CSSValue& value) {
  CSSPropertyRef ref(name, state.GetDocument());
  DCHECK(ref.IsValid());

  ApplyProperty(ref.GetProperty(), state, value);
}

void StyleBuilder::ApplyProperty(const CSSProperty& property,
                                 StyleResolverState& state,
                                 const CSSValue& value) {
  DCHECK(!Variable::IsStaticInstance(property))
      << "Please use a CustomProperty instance to apply custom properties";

  CSSPropertyID id = property.PropertyID();
  bool is_inherited = property.IsInherited();
  if (id != CSSPropertyVariable && (value.IsVariableReferenceValue() ||
                                    value.IsPendingSubstitutionValue())) {
    bool omit_animation_tainted =
        CSSAnimations::IsAnimationAffectingProperty(property);
    const CSSValue* resolved_value =
        CSSVariableResolver(state).ResolveVariableReferences(
            id, value, omit_animation_tainted);
    ApplyProperty(property, state, *resolved_value);

    if (!state.Style()->HasVariableReferenceFromNonInheritedProperty() &&
        !is_inherited)
      state.Style()->SetHasVariableReferenceFromNonInheritedProperty();
    return;
  }

  DCHECK(!property.IsShorthand())
      << "Shorthand property id = " << id << " wasn't expanded at parsing time";

  bool is_inherit = state.ParentNode() && value.IsInheritedValue();
  bool is_initial = value.IsInitialValue() ||
                    (!state.ParentNode() && value.IsInheritedValue());

  // isInherit => !isInitial && isInitial => !isInherit
  DCHECK(!is_inherit || !is_initial);
  // isInherit => (state.parentNode() && state.parentStyle())
  DCHECK(!is_inherit || (state.ParentNode() && state.ParentStyle()));

  if (!state.ApplyPropertyToRegularStyle() &&
      (!state.ApplyPropertyToVisitedLinkStyle() ||
       !property.IsValidForVisitedLink())) {
    // Limit the properties that can be applied to only the ones honored by
    // :visited.
    return;
  }

  if (is_inherit && !state.ParentStyle()->HasExplicitlyInheritedProperties() &&
      !is_inherited) {
    state.ParentStyle()->SetHasExplicitlyInheritedProperties();
  } else if (value.IsUnsetValue()) {
    DCHECK(!is_inherit && !is_initial);
    if (is_inherited)
      is_inherit = true;
    else
      is_initial = true;
  }

  if (IsCSSNavigationProperty(id)) {
    ApplyCSSNavigationProperty(id, state, value, is_initial, is_inherit);
    return;
  }

  // CSSPropertyVariable currently handles initial/inherit inside ApplyValue.
  DCHECK(id != CSSPropertyVariable || !is_initial);
  DCHECK(id != CSSPropertyVariable || !is_inherit);

  if (is_initial)
    ToLonghand(property).ApplyInitial(state);
  else if (is_inherit)
    ToLonghand(property).ApplyInherit(state);
  else
    ToLonghand(property).ApplyValue(state, value);
}

}  // namespace blink
