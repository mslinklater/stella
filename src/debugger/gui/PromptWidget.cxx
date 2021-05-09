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
// Copyright (c) 1995-2021 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================

#include "ScrollBarWidget.hxx"
#include "FBSurface.hxx"
#include "Font.hxx"
#include "StellaKeys.hxx"
#include "Version.hxx"
#include "OSystem.hxx"
#include "Debugger.hxx"
#include "DebuggerDialog.hxx"
#include "DebuggerParser.hxx"
#include "EventHandler.hxx"

#include "PromptWidget.hxx"
#include "CartDebug.hxx"

#define PROMPT  "> "

// Uncomment the following to give full-line cut/copy/paste
// Note that this will be removed eventually, when we implement proper cut/copy/paste
#define PSEUDO_CUT_COPY_PASTE

// TODO: Github issue #361
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
PromptWidget::PromptWidget(GuiObject* boss, const GUI::Font& font,
                           int x, int y, int w, int h)
  : Widget(boss, font, x, y, w - ScrollBarWidget::scrollBarWidth(font), h),
    CommandSender(boss),
    _historySize{0},
    _historyIndex{0},
    _historyLine{0},
    _firstTime{true},
    _exitedEarly{false}
{
  _flags = Widget::FLAG_ENABLED | Widget::FLAG_CLEARBG | Widget::FLAG_RETAIN_FOCUS |
           Widget::FLAG_WANTS_TAB | Widget::FLAG_WANTS_RAWDATA;
  _textcolor = kTextColor;
  _bgcolor = kWidColor;
  _bgcolorlo = kDlgColor;

  _kConsoleCharWidth  = font.getMaxCharWidth();
  _kConsoleCharHeight = font.getFontHeight();
  _kConsoleLineHeight = _kConsoleCharHeight + 2;

  // Calculate depending values
  _lineWidth = (_w - ScrollBarWidget::scrollBarWidth(_font) - 2) / _kConsoleCharWidth;
  _linesPerPage = (_h - 2) / _kConsoleLineHeight;
  _linesInBuffer = kBufferSize / _lineWidth;

  // Add scrollbar
  _scrollBar = new ScrollBarWidget(boss, font, _x + _w, _y,
                                   ScrollBarWidget::scrollBarWidth(_font), _h);
  _scrollBar->setTarget(this);

  // Init colors
  _inverse = false;

  clearScreen();

  addFocusWidget(this);
  setHelpAnchor("PromptTab", true);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::drawWidget(bool hilite)
{
//cerr << "PromptWidget::drawWidget\n";
  ColorId fgcolor, bgcolor;
  FBSurface& s = _boss->dialog().surface();

  // Draw text
  int start = _scrollLine - _linesPerPage + 1;
  int y = _y + 2;

  for (int line = 0; line < _linesPerPage; ++line)
  {
    int x = _x + 1;
    for (int column = 0; column < _lineWidth; ++column) {
      int c = buffer((start + line) * _lineWidth + column);

      if(c & (1 << 17))  // inverse video flag
      {
        fgcolor = _bgcolor;
        bgcolor = ColorId((c & 0x1ffff) >> 8);
        s.fillRect(x, y, _kConsoleCharWidth, _kConsoleCharHeight, bgcolor);
      }
      else
        fgcolor = ColorId(c >> 8);

      s.drawChar(_font, c & 0x7f, x, y, fgcolor);
      x += _kConsoleCharWidth;
    }
    y += _kConsoleLineHeight;
  }

  // Draw the caret
  drawCaret();

  // Draw the scrollbar
  _scrollBar->draw();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::handleMouseDown(int x, int y, MouseButton b, int clickCount)
{
//  cerr << "PromptWidget::handleMouseDown\n";
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::handleMouseWheel(int x, int y, int direction)
{
  _scrollBar->handleMouseWheel(x, y, direction);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::printPrompt()
{
  string watches = instance().debugger().showWatches();
  if(watches.length() > 0)
    print(watches);

  print(PROMPT);
  _promptStartPos = _promptEndPos = _currentPos;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool PromptWidget::handleText(char text)
{
  if(text >= 0)
  {
    // FIXME - convert this class to inherit from EditableWidget
    for(int i = _promptEndPos - 1; i >= _currentPos; i--)
      buffer(i + 1) = buffer(i);
    _promptEndPos++;
    putcharIntern(text);
    scrollToCurrent();
  }
  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool PromptWidget::handleKeyDown(StellaKey key, StellaMod mod)
{
  bool handled = true;
  bool dirty = true;

  if(key != KBDK_TAB && !StellaModTest::isShift(mod))
    _tabCount = -1;

  // Uses normal edit events + special prompt events
  Event::Type event = instance().eventHandler().eventForKey(EventMode::kEditMode, key, mod);
  if(event == Event::NoType)
    event = instance().eventHandler().eventForKey(EventMode::kPromptMode, key, mod);

  switch(event)
  {
    case Event::EndEdit:
    {
      if(execute())
        return true;

      printPrompt();
      break;
    }

    case Event::UINavNext:
      dirty = autoComplete(+1);
      break;

    case Event::UINavPrev:
      dirty = autoComplete(-1);
      break;

    case Event::UILeft:
      historyScroll(-1);
      break;

    case Event::UIRight:
      historyScroll(+1);
      break;

    case Event::Backspace:
      if(_currentPos > _promptStartPos)
        killChar(-1);

      scrollToCurrent();
      break;

    case Event::Delete:
      killChar(+1);
      break;

    case Event::MoveHome:
      _currentPos = _promptStartPos;
      break;

    case Event::MoveEnd:
      _currentPos = _promptEndPos;
      break;

    case Event::MoveRightChar:
      if(_currentPos < _promptEndPos)
        _currentPos++;
      break;

    case Event::MoveLeftChar:
      if(_currentPos > _promptStartPos)
        _currentPos--;
      break;

    case Event::DeleteRightWord:
      killChar(+1);
      break;

    case Event::DeleteEnd:
      killLine(+1);
      break;

    case Event::DeleteHome:
      killLine(-1);
      break;

    case Event::DeleteLeftWord:
      killWord();
      break;

    case Event::UIUp:
      if(_scrollLine <= _firstLineInBuffer + _linesPerPage - 1)
        break;

      _scrollLine -= 1;
      updateScrollBuffer();
      break;

    case Event::UIDown:
      // Don't scroll down when at bottom of buffer
      if(_scrollLine >= _promptEndPos / _lineWidth)
        break;

      _scrollLine += 1;
      updateScrollBuffer();
      break;

    case Event::UIPgUp:
      // Don't scroll up when at top of buffer
      if(_scrollLine < _linesPerPage)
        break;

      _scrollLine -= _linesPerPage - 1;
      if(_scrollLine < _firstLineInBuffer + _linesPerPage - 1)
        _scrollLine = _firstLineInBuffer + _linesPerPage - 1;
      updateScrollBuffer();
      break;

    case Event::UIPgDown:
      // Don't scroll down when at bottom of buffer
      if(_scrollLine >= _promptEndPos / _lineWidth)
        break;

      _scrollLine += _linesPerPage - 1;
      if(_scrollLine > _promptEndPos / _lineWidth)
        _scrollLine = _promptEndPos / _lineWidth;
      updateScrollBuffer();
      break;

    case Event::UIHome:
      _scrollLine = _firstLineInBuffer + _linesPerPage - 1;
      updateScrollBuffer();
      break;

    case Event::UIEnd:
      _scrollLine = _promptEndPos / _lineWidth;
      if(_scrollLine < _linesPerPage - 1)
        _scrollLine = _linesPerPage - 1;
      updateScrollBuffer();
      break;

    //case Event::SelectAll:
    //  textSelectAll();
    //  break;

    case Event::Cut:
      textCut();
      break;

    case Event::Copy:
      textCopy();
      break;

    case Event::Paste:
      textPaste();
      break;

    default:
      handled = false;
      dirty = false;
      break;
  }

  // Take care of changes made above
  if(dirty)
    setDirty();

  return handled;
}

#if 0
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::insertIntoPrompt(const char* str)
{
  Int32 l = (Int32)strlen(str);
  for(Int32 i = _promptEndPos - 1; i >= _currentPos; i--)
    buffer(i + l) = buffer(i);

  for(Int32 j = 0; j < l; ++j)
  {
    _promptEndPos++;
    putcharIntern(str[j]);
  }
}
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::handleCommand(CommandSender* sender, int cmd,
                                 int data, int id)
{
  if(cmd == GuiObject::kSetPositionCmd)
  {
    int newPos = int(data) + _linesPerPage - 1 + _firstLineInBuffer;
    if (newPos != _scrollLine)
    {
      _scrollLine = newPos;
      setDirty();
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::loadConfig()
{
  // Show the prompt the first time we draw this widget
  if(_firstTime)
  {
    _firstTime = false;

    // Display greetings & prompt
    string version = string("Stella ") + STELLA_VERSION + "\n";
    print(version);
    print(PROMPT);

    // Take care of one-time debugger stuff
    // fill the history from the saved breaks, traps and watches commands
    StringList history;
    print(instance().debugger().autoExec(&history));
    for(uInt32 i = 0; i < history.size(); ++i)
    {
      addToHistory(history[i].c_str());
    }
    history.clear();
    print(instance().debugger().cartDebug().loadConfigFile() + "\n");
    print(instance().debugger().cartDebug().loadListFile() + "\n");
    print(instance().debugger().cartDebug().loadSymbolFile() + "\n");
    if(instance().settings().getBool("dbg.logbreaks"))
      print(DebuggerParser::inverse(" logBreaks enabled \n"));
    print(PROMPT);

    _promptStartPos = _promptEndPos = _currentPos;
    _exitedEarly = false;
  }
  else if(_exitedEarly)
  {
    printPrompt();
    _exitedEarly = false;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int PromptWidget::getWidth() const
{
  return _w + ScrollBarWidget::scrollBarWidth(_font);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::killChar(int direction)
{
  if(direction == -1)    // Delete previous character (backspace)
  {
    if(_currentPos <= _promptStartPos)
      return;

    _currentPos--;
    for (int i = _currentPos; i < _promptEndPos; i++)
      buffer(i) = buffer(i + 1);

    buffer(_promptEndPos) = ' ';
    _promptEndPos--;
  }
  else if(direction == 1)    // Delete next character (delete)
  {
    if(_currentPos >= _promptEndPos)
      return;

    // There are further characters to the right of cursor
    if(_currentPos + 1 <= _promptEndPos)
    {
      for (int i = _currentPos; i < _promptEndPos; i++)
        buffer(i) = buffer(i + 1);

      buffer(_promptEndPos) = ' ';
      _promptEndPos--;
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::killLine(int direction)
{
  if(direction == -1)  // erase from current position to beginning of line
  {
    int count = _currentPos - _promptStartPos;
    if(count > 0)
      for (int i = 0; i < count; i++)
       killChar(-1);
  }
  else if(direction == 1)  // erase from current position to end of line
  {
    for (int i = _currentPos; i < _promptEndPos; i++)
      buffer(i) = ' ';

    _promptEndPos = _currentPos;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::killWord()
{
  int cnt = 0;
  bool space = true;
  while (_currentPos > _promptStartPos)
  {
    if ((buffer(_currentPos - 1) & 0xff) == ' ')
    {
      if (!space)
        break;
    }
    else
      space = false;

    _currentPos--;
    cnt++;
  }

  for (int i = _currentPos; i < _promptEndPos; i++)
    buffer(i) = buffer(i + cnt);

  buffer(_promptEndPos) = ' ';
  _promptEndPos -= cnt;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::textSelectAll()
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
string PromptWidget::getLine()
{
#if defined(PSEUDO_CUT_COPY_PASTE)
  assert(_promptEndPos >= _promptStartPos);
  int len = _promptEndPos - _promptStartPos;
  string text;

  // Copy current line to text
  for(int i = 0; i < len; i++)
    text += buffer(_promptStartPos + i) & 0x7f;

  return text;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::textCut()
{
#if defined(PSEUDO_CUT_COPY_PASTE)
  string text = getLine();

  instance().eventHandler().copyText(text);

  // Remove the current line
  _currentPos = _promptStartPos;
  killLine(1);  // to end of line
  _promptEndPos = _currentPos;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::textCopy()
{
#if defined(PSEUDO_CUT_COPY_PASTE)
  string text = getLine();

  instance().eventHandler().copyText(text);
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::textPaste()
{
#if defined(PSEUDO_CUT_COPY_PASTE)
  string text;

  // Remove the current line
  _currentPos = _promptStartPos;
  killLine(1);  // to end of line

  instance().eventHandler().pasteText(text);
  print(text);
  _promptEndPos = _currentPos;
#endif
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::addToHistory(const char* str)
{
  // TOOD: do not add duplicates, remove oldest
#if defined(BSPF_WINDOWS)
  strncpy_s(_history[_historyIndex], kLineBufferSize, str, kLineBufferSize - 1);
#else
  strncpy(_history[_historyIndex], str, kLineBufferSize - 1);
#endif
  _historyIndex = (_historyIndex + 1) % kHistorySize;
  _historyLine = 0;

  if (_historySize < kHistorySize)
    _historySize++;
}

#if 0 // FIXME
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int PromptWidget::compareHistory(const char *histLine)
{
  return 1;
}
#endif

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::historyScroll(int direction)
{
  if (_historySize == 0)
    return;

  if (_historyLine == 0 && direction > 0)
  {
    int i;
    for (i = 0; i < _promptEndPos - _promptStartPos; i++)
      _history[_historyIndex][i] = buffer(_promptStartPos + i); //FIXME: int to char??

    _history[_historyIndex][i] = '\0';
  }

  // Advance to the next line in the history
  int line = _historyLine + direction;
  if(line < 0)
    line += _historySize + 1;
  line %= (_historySize + 1);

  // If they press arrow-up with anything in the buffer, search backwards
  // in the history.
  /*
  if(direction < 0 && _currentPos > _promptStartPos) {
    for(;line > 0; line--) {
      if(compareHistory(_history[line]) == 0)
        break;
    }
  }
  */

  _historyLine = line;

  // Remove the current user text
  _currentPos = _promptStartPos;
  killLine(1);  // to end of line

  // ... and ensure the prompt is visible
  scrollToCurrent();

  // Print the text from the history
  int idx;
  if (_historyLine > 0)
    idx = (_historyIndex - _historyLine + _historySize) % _historySize;
  else
    idx = _historyIndex;

  for (int i = 0; i < kLineBufferSize && _history[idx][i] != '\0'; i++)
    putcharIntern(_history[idx][i]);

  _promptEndPos = _currentPos;

  // Ensure once more the caret is visible (in case of very long history entries)
  scrollToCurrent();

  setDirty();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool PromptWidget::execute()
{
  nextLine();

  assert(_promptEndPos >= _promptStartPos);
  int len = _promptEndPos - _promptStartPos;

  if(len > 0)
  {
    // Copy the user input to command
    string command;
    for(int i = 0; i < len; i++)
      command += buffer(_promptStartPos + i) & 0x7f;

    // Add the input to the history
    addToHistory(command.c_str());

    // Pass the command to the debugger, and print the result
    string result = instance().debugger().run(command);

    // This is a bit of a hack
    // Certain commands remove the debugger dialog from underneath us,
    // so we shouldn't print any messages
    // Those commands will return '_EXIT_DEBUGGER' as their result
    if(result == "_EXIT_DEBUGGER")
    {
      _exitedEarly = true;
      return true;
    }
    else if(result == "_NO_PROMPT")
      return true;
    else if(result != "")
      print(result + "\n");
  }
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool PromptWidget::autoComplete(int direction)
{
  // Tab completion: we complete either commands or labels, but not
  // both at once.

  if(_currentPos <= _promptStartPos)
    return false; // no input

  scrollToCurrent();

  int len = _promptEndPos - _promptStartPos;

  if(_tabCount != -1)
    len = int(strlen(_inputStr));
  if(len > 255)
    len = 255;

  int lastDelimPos = -1;
  char delimiter = '\0';

  for(int i = 0; i < len; i++)
  {
    // copy the input at first tab press only
    if(_tabCount == -1)
      _inputStr[i] = buffer(_promptStartPos + i) & 0x7f;
    // whitespace characters
    if(strchr("{*@<> =[]()+-/&|!^~%", _inputStr[i]))
    {
      lastDelimPos = i;
      delimiter = _inputStr[i];
    }
}
  if(_tabCount == -1)
    _inputStr[len] = '\0';

  StringList list;

  if(lastDelimPos == -1)
    // no delimiters, do only command completion:
    instance().debugger().parser().getCompletions(_inputStr, list);
  else
  {
    size_t strLen = len - lastDelimPos - 1;
    // do not show ALL commands/labels without any filter as it makes no sense
    if(strLen > 0)
    {
      // Special case for 'help' command
      if(BSPF::startsWithIgnoreCase(_inputStr, "help"))
        instance().debugger().parser().getCompletions(_inputStr + lastDelimPos + 1, list);
      else
      {
        // we got a delimiter, so this must be a label or a function
        const Debugger& dbg = instance().debugger();

        dbg.cartDebug().getCompletions(_inputStr + lastDelimPos + 1, list);
        dbg.getCompletions(_inputStr + lastDelimPos + 1, list);
      }
    }

  }
  if(list.size() < 1)
    return false;
  sort(list.begin(), list.end());

  if(direction < 0)
  {
    if(--_tabCount < 0)
      _tabCount = int(list.size()) - 1;
  }
  else
    _tabCount = (++_tabCount) % list.size();

  nextLine();
  _currentPos = _promptStartPos;
  killLine(1);  // kill whole line

  // start with-autocompleted, fixed string...
  for(int i = 0; i < lastDelimPos; i++)
    putcharIntern(_inputStr[i]);
  if(lastDelimPos > 0)
    putcharIntern(delimiter);

  // ...and add current autocompletion string
  print(list[_tabCount]);
  putcharIntern(' ');
  _promptEndPos = _currentPos;

  return true;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::nextLine()
{
  // Reset colors every line, so I don't have to remember to do it myself
  _textcolor = kTextColor;
  _inverse = false;

  int line = _currentPos / _lineWidth;
  if (line == _scrollLine)
    _scrollLine++;

  _currentPos = (line + 1) * _lineWidth;

  updateScrollBuffer();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Call this (at least) when the current line changes or when a new line is added
void PromptWidget::updateScrollBuffer()
{
  int lastchar = std::max(_promptEndPos, _currentPos);
  int line = lastchar / _lineWidth;
  int numlines = (line < _linesInBuffer) ? line + 1 : _linesInBuffer;
  int firstline = line - numlines + 1;

  if (firstline > _firstLineInBuffer)
  {
    // clear old line from buffer
    for (int i = lastchar; i < (line+1) * _lineWidth; ++i)
      buffer(i) = ' ';

    _firstLineInBuffer = firstline;
  }

  _scrollBar->_numEntries = numlines;
  _scrollBar->_currentPos = _scrollBar->_numEntries - (line - _scrollLine + _linesPerPage);
  _scrollBar->_entriesPerPage = _linesPerPage;
  _scrollBar->recalc();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// TODO: rewrite this (cert-dcl50-cpp)
int PromptWidget::printf(const char* format, ...)  // NOLINT
{
  va_list argptr;

  va_start(argptr, format);
  int count = this->vprintf(format, argptr);
  va_end (argptr);
  return count;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
int PromptWidget::vprintf(const char* format, va_list argptr)
{
  char buf[2048];  // NOLINT  (will be rewritten soon)
  int count = std::vsnprintf(buf, sizeof(buf), format, argptr);

  print(buf);
  return count;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::putcharIntern(int c)
{
  if (c == '\n')
    nextLine();
  else if(c & 0x80) { // set foreground color to TIA color
                      // don't print or advance cursor
    _textcolor = ColorId((c & 0x7f) << 1);
  }
  else if(c && c < 0x1e) { // first actual character is large dash
    // More colors (the regular GUI ones)
    _textcolor = ColorId(c + 0x100);
  }
  else if(c == 0x7f) { // toggle inverse video (DEL char)
    _inverse = !_inverse;
  }
  else if(isprint(c))
  {
    buffer(_currentPos) = c | (_textcolor << 8) | (_inverse << 17);
    _currentPos++;
    if ((_scrollLine + 1) * _lineWidth == _currentPos)
    {
      _scrollLine++;
      updateScrollBuffer();
    }
  }
  setDirty();
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::print(const string& str)
{
  for(char c: str)
    putcharIntern(c);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::drawCaret()
{
//cerr << "PromptWidget::drawCaret()\n";
  FBSurface& s = _boss->dialog().surface();
  int line = _currentPos / _lineWidth;

  // Don't draw the cursor if it's not in the current view
  if(_scrollLine < line)
    return;

  int displayLine = line - _scrollLine + _linesPerPage - 1;
  int x = _x + 1 + (_currentPos % _lineWidth) * _kConsoleCharWidth;
  int y = _y + displayLine * _kConsoleLineHeight;

  char c = buffer(_currentPos); //FIXME: int to char??
  s.fillRect(x, y, _kConsoleCharWidth, _kConsoleLineHeight, kTextColor);
  s.drawChar(_font, c, x, y + 2, kBGColor);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::scrollToCurrent()
{
  int line = _promptEndPos / _lineWidth;

  if (line + _linesPerPage <= _scrollLine)
  {
    // TODO - this should only occur for long edit lines, though
  }
  else if (line > _scrollLine)
  {
    _scrollLine = line;
    updateScrollBuffer();
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
string PromptWidget::saveBuffer(const FilesystemNode& file)
{
  stringstream out;
  for(int start = 0; start < _promptStartPos; start += _lineWidth)
  {
    int end = start + _lineWidth - 1;

    // Look for first non-space, printing char from end of line
    while( char(_buffer[end] & 0xff) <= ' ' && end >= start)
      end--;

    // Spit out the line minus its trailing junk
    // Strip off any color/inverse bits
    for(int j = start; j <= end; ++j)
      out << char(_buffer[j] & 0xff);

    // add a \n
    out << endl;
  }

  try {
    if(file.write(out) > 0)
      return "saved " + file.getShortPath() + " OK";
  }
  catch(...) { }

  return "unable to save session";
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void PromptWidget::clearScreen()
{
  // Initialize start position
  _currentPos = 0;
  _scrollLine = _linesPerPage - 1;
  _firstLineInBuffer = 0;
  _promptStartPos = _promptEndPos = -1;
  memset(_buffer, 0, kBufferSize * sizeof(int));

  if(!_firstTime)
    updateScrollBuffer();
}
