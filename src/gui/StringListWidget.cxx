//============================================================================
//
//   SSSS    tt          lll  lll       
//  SS  SS   tt           ll   ll        
//  SS     tttttt  eeee   ll   ll   aaaa 
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2016 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id$
//============================================================================

#include "bspf.hxx"
#include "Settings.hxx"
#include "ScrollBarWidget.hxx"
#include "StringListWidget.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
StringListWidget::StringListWidget(GuiObject* boss, const GUI::Font& font,
                                   int x, int y, int w, int h, bool hilite)
  : ListWidget(boss, font, x, y, w, h,
               boss->instance().settings().getInt("listdelay") >= 300),
    _hilite(hilite)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void StringListWidget::setList(const StringList& list)
{
  _list = list;

  ListWidget::recalc();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void StringListWidget::drawWidget(bool hilite)
{
  FBSurface& s = _boss->dialog().surface();
  int i, pos, len = int(_list.size());

  // Draw a thin frame around the list.
  s.hLine(_x, _y, _x + _w - 1, kColor);
  s.hLine(_x, _y + _h - 1, _x + _w - 1, kShadowColor);
  s.vLine(_x, _y, _y + _h - 1, kColor);

  // Draw the list items
  for (i = 0, pos = _currentPos; i < _rows && pos < len; i++, pos++)
  {
    const int y = _y + 2 + _fontHeight * i;

    // Draw the selected item inverted, on a highlighted background.
    if (_selectedItem == pos && _hilite)
    {
      if (_hasFocus && !_editMode)
        s.fillRect(_x + 1, _y + 1 + _fontHeight * i, _w - 1, _fontHeight, kTextColorHi);
      else
        s.frameRect(_x + 1, _y + 1 + _fontHeight * i, _w - 1, _fontHeight, kTextColorHi);
    }

    GUI::Rect r(getEditRect());
    if (_selectedItem == pos && _editMode)
    {
      adjustOffset();
      s.drawString(_font, editString(), _x + r.left, y, r.width(), kTextColor,
                   kTextAlignLeft, -_editScrollOffset, false);
    }
    else
      s.drawString(_font, _list[pos], _x + r.left, y, r.width(), kTextColor);
  }

  // Only draw the caret while editing, and if it's in the current viewport
  if(_editMode && (_selectedItem >= _scrollBar->_currentPos) &&
    (_selectedItem < _scrollBar->_currentPos + _rows))
    drawCaret();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
GUI::Rect StringListWidget::getEditRect() const
{
  GUI::Rect r(2, 1, _w - 2, _fontHeight);
  const int offset = (_selectedItem - _currentPos) * _fontHeight;
  r.top += offset;
  r.bottom += offset;
	
  return r;
}
