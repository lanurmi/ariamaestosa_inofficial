/*
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef RENDERER_WXWIDGETS
#include "Config.h"

#include "Renderers/wxRenderPane.h"
#include "AriaCore.h"

#include "wx/wx.h"
#include "wx/brush.h"

#include <iostream>
#include <cmath>

#include "GUI/MainFrame.h"
#include "Config.h"

namespace AriaMaestosa {

// ==========================================================================================
// ==========================================================================================

wxRenderPane::wxRenderPane(MainFrame* mainFrame, int* args) :
    wxPanel(mainFrame, wxID_ANY,  wxDefaultPosition, wxDefaultSize)
{

    

}

wxRenderPane::~wxRenderPane()
{
}



void wxRenderPane::resized(wxSizeEvent& evt)
{
    wxPanel::OnSize(evt);
    if (getMainFrame()->getSequenceAmount()>0) DisplayFrame::updateVerticalScrollbar();
}

int wxRenderPane::getWidth()
{

    if (Display::isVisible()) return GetSize().x;
    else return 795; // default value
}

int wxRenderPane::getHeight()
{
    // FIXME - is it really necessary to check if it's visible?
    if (Display::isVisible()) return GetSize().y;
    else return 550; // approximately default
}

bool wxRenderPane::prepareFrame()
{
    if (!GetParent()->IsShown()) return false; 
    return true;
}

void wxRenderPane::beginFrame()
{
    Display::renderDC -> SetBackground( *wxBLACK_BRUSH );
    Display::renderDC -> Clear();
}
void wxRenderPane::endFrame()
{
}


}

#endif