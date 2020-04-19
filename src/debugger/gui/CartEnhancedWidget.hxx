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
// Copyright (c) 1995-2020 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//============================================================================

#ifndef CART_ENHANCED_WIDGET_HXX
#define CART_ENHANCED_WIDGET_HXX

class CartridgeEnhanced;
class PopUpWidget;

namespace GUI {
  class Font;
}

#include "CartDebugWidget.hxx"

class CartEnhancedWidget : public CartDebugWidget
{
  public:
    CartEnhancedWidget(GuiObject* boss, const GUI::Font& lfont,
                       const GUI::Font& nfont,
                       int x, int y, int w, int h,
                       CartridgeEnhanced& cart);
    virtual ~CartEnhancedWidget() = default;

  protected:
    void initialize();

    virtual size_t size();

    virtual string manufacturer() = 0;

    virtual string description();

    virtual int descriptionLines();

    virtual string ramDescription();

    virtual string romDescription();

    virtual void bankSelect(int& ypos);

    virtual string hotspotStr(int bank, int segment = 0);

    virtual int bankSegs(); // { return myCart.myBankSegs; }

    void saveOldState() override;
    void loadConfig() override;

    void handleCommand(CommandSender* sender, int cmd, int data, int id) override;

    string bankState() override;

    // start of functions for Cartridge RAM tab
    uInt32 internalRamSize() override;
    uInt32 internalRamRPort(int start) override;
    string internalRamDescription() override;
    const ByteArray& internalRamOld(int start, int count) override;
    const ByteArray& internalRamCurrent(int start, int count) override;
    void internalRamSetValue(int addr, uInt8 value) override;
    uInt8 internalRamGetValue(int addr) override;
    string internalRamLabel(int addr) override;
    // end of functions for Cartridge RAM tab

  protected:
    enum { kBankChanged = 'bkCH' };

    struct CartState {
      ByteArray internalRam;
      ByteArray banks;
    };
    CartState myOldState;

    CartridgeEnhanced& myCart;

    // Distance between two hotspots
    int myHotspotDelta{1};

    std::unique_ptr<PopUpWidget* []> myBankWidgets{nullptr};


    // Display all addresses based on this
    static constexpr uInt16 ADDR_BASE = 0xF000;

  private:
    // Following constructors and assignment operators not supported
    CartEnhancedWidget() = delete;
    CartEnhancedWidget(const CartEnhancedWidget&) = delete;
    CartEnhancedWidget(CartEnhancedWidget&&) = delete;
    CartEnhancedWidget& operator=(const CartEnhancedWidget&) = delete;
    CartEnhancedWidget& operator=(CartEnhancedWidget&&) = delete;
};

#endif
