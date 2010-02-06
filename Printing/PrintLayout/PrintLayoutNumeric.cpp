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

#include "Printing/PrintLayout/PrintLayoutNumeric.h"

#include "Printing/AriaPrintable.h"
#include "Printing/PrintableSequence.h"

using namespace AriaMaestosa;

// -----------------------------------------------------------------------------------------------------------------

PrintLayoutNumeric::PrintLayoutNumeric(PrintableSequence* sequence)
{
    m_sequence = sequence;
}

// -----------------------------------------------------------------------------------------------------------------

// FIXME: this sets the coords of each layout element of a LayoutLine, but is called for every LineTrackRef!
// Does this mean that layout elements coords are calculated multiple times when there's more than one track?
void PrintLayoutNumeric::placeTrackAndElementsWithinCoords(const int trackID, LayoutLine& line, LineTrackRef& track,
                                                        int x0, const int y0, const int x1, const int y1, bool show_measure_number)
{
    std::cout << "= placeTrackAndElementsWithinCoords =\n";
    
    assertExpr(x0, >=, 0);
    assertExpr(x1, >=, 0);
    assertExpr(y0, >=, 0);
    assertExpr(y1, >=, 0);
    
    track.x0 = x0;
    track.x1 = x1;
    track.y0 = y0;
    track.y1 = y1;
    
    track.show_measure_number = show_measure_number;
    
    if (&line.getLineTrackRef(trackID) != &track) std::cerr << "LineTrackRef is not the right one!!!!!!!!!\n";
    // std::cout << "coords for track " << line.getTrack(trackID) << " : " << x0 << ", " << y0 << ", " << x1 << ", " << y1 << std::endl;
    
    // 2 spaces allocated for left area of the line
    //track.pixel_width_of_an_unit = (float)(x1 - x0) / (float)(line.width_in_units+2);
    //std::cout << "    Line has " << line.width_in_units << " units. pixel_width_of_an_unit=" << track.pixel_width_of_an_unit << "\n";
    
    track.layoutElementsAmount = line.getLayoutElementCount();
    
    // find total amount of units
    /*
     int totalUnitCount = 0;
     for (int currentLayoutElement=0; currentLayoutElement<track.layoutElementsAmount; currentLayoutElement++)
     {
     totalUnitCount += line.layoutElements[currentLayoutElement].width_in_units;
     }
     int pixel_width_of_an_unit = (x1 - x0) / totalUnitCount;
     */
    
    // find total needed width (just in case we have more, then we can spread things a bit!)
    int totalNeededWidth = 0;
    for (int currentLayoutElement=0; currentLayoutElement<track.layoutElementsAmount; currentLayoutElement++)
    {
        totalNeededWidth += line.getLayoutElement(currentLayoutElement).width_in_print_units;
        totalNeededWidth += MARGIN_AT_MEASURE_BEGINNING;
    }
    
    const int availableWidth = (x1 - x0);
    assertExpr(totalNeededWidth, <=, availableWidth);
    
    float zoom = (float)availableWidth / (float)totalNeededWidth;
    if (zoom > 1.5f) zoom = 1.5f; // prevent zooming too much, will look weird
    
    // init coords of each layout element
    int xloc = 0;
    
    for (int currentLayoutElement=0; currentLayoutElement<track.layoutElementsAmount; currentLayoutElement++)
    {
        if (currentLayoutElement  > 0)
        {
            // The margin at the end is provided by the RelativePlacementManager IIRC (FIXME: ugly)
            xloc += line.getLayoutElement(currentLayoutElement-1).width_in_print_units*zoom + MARGIN_AT_MEASURE_BEGINNING;
        }
        
        std::cout << "    - Setting coords of element " << currentLayoutElement
        << " of current line. xfrom = " << x0 + xloc << "\n";
        
        line.getLayoutElement(currentLayoutElement).setXFrom( x0 + xloc );
        
        if (currentLayoutElement > 0)
        {
            line.getLayoutElement(currentLayoutElement-1).setXTo( line.getLayoutElement(currentLayoutElement).getXFrom() );
        }
    }
    // for last
    xloc += line.getLayoutElement(line.getLayoutElementCount()-1).width_in_print_units*zoom + MARGIN_AT_MEASURE_BEGINNING;
    line.getLayoutElement(line.getLayoutElementCount()-1).setXTo( x0 + xloc );
    
    // check if there is space left between the last element and the end of the line.
    if (x0 + xloc < x1 - 300 )
    {
        line.getLayoutElement(line.getLayoutElementCount()-1).render_end_bar = true;
    }
    
    //assertExpr(line.width_in_units,>,0);
}

// -----------------------------------------------------------------------------------------------------------------

void PrintLayoutNumeric::divideLineAmongTracks(LayoutLine& line, const int x0, const int y0, const int x1,
                                               const int y1, int margin_below, int margin_above)
{
    const int trackAmount = line.getTrackAmount();
    
    std::cout << "Line given coords " << x0 << ", " << y0 << " to " << x1 << ", " << y1 << std::endl;
    std::cout << "==divideLineAmongTracks==\n";
    
    line.x0 = x0;
    line.y0 = y0;
    line.x1 = x1;
    line.y1 = y1;
    line.margin_below = margin_below;
    line.margin_above = margin_above;
    
    // ---- empty space around whole line
    const float height = (float)(y1 - y0);// - ( trackAmount>1 and not last_of_page ? 100 : 0 );
    
    if (height < 0.0001) return; // empty line. TODO : todo - draw empty bars to show there's something?
    
    // make sure margins are within acceptable bounds
    if (margin_below > height/2) margin_below = height/5;
    if (margin_above > height/2) margin_above = height/5;
    
    
    const int my0 = y0 + margin_above;
    
    // ---- Determine tracks positions and sizes
    
    int nonEmptyTrackAmount = 0; // empty tracks must not be counted
    for(int n=0; n<trackAmount; n++)
    {        
        if (line.height_percent[n] > 0) nonEmptyTrackAmount++;
    }
    
    // FIXME : this is layout, should go in PrintLayout.cpp ?
    // space between individual tracks
    const int space_between_tracks = nonEmptyTrackAmount > 1 ? 150 : 0;
    
    float current_y = my0;
    int nonEmptyID = 0;
    for (int n=0; n<trackAmount; n++)
    {        
        //EditorPrintable* editorPrintable = m_sequence->getEditorPrintable(n);
        
        // skip empty tracks
        if (line.height_percent[n] == 0) continue;
        
        // determine how much vertical space is allocated for this track
        const float track_height = (height - margin_below - margin_above) * line.height_percent[n]/100.0f;
        std::cout << "track_height=" << track_height << " (margin_below=" << margin_below << " margin_above=" << margin_above
                  << "space_between_tracks=" << space_between_tracks << ")\n";
        
        // margin space above and below each track are given by simple formula 'space_between_tracks*position' where
        // position ranges from 0 to 1. However, this formula doesn't make the space between 2 tracks equal to
        // 'space_between_tracks' in all cases (especially depending on track amount), so the following correction
        // needs to be applied (FIXME : can it be harder to understand what i'm doing here...)
        const float adjustMarginRatio = (float)(nonEmptyTrackAmount-1) / (float)(nonEmptyTrackAmount);
        
        const float position =
        nonEmptyTrackAmount-1 == 0 ? 0.0f : // avoid division by zero
        (float)nonEmptyID / (float)(nonEmptyTrackAmount-1);
        const float space_above_line = space_between_tracks*position*adjustMarginRatio;
        const float space_below_line = space_between_tracks*(1.0-position)*adjustMarginRatio;
        
        placeTrackAndElementsWithinCoords(n, line, line.getLineTrackRef(n),
                                          x0, current_y + space_above_line,
                                          x1, current_y + track_height - space_below_line,
                                          n==0);
        
        std::cout << "%%%% setting track coords " << n  << " : " << x0 << ", " << (current_y + space_above_line)
            << " to "  << x1 << ", "<< (current_y + track_height - space_below_line)
            << " ( space_above_line=" << space_above_line << " space_below_line=" << space_below_line
            << " track_height=" << track_height << ")" <<  std::endl;
        
        
        current_y += track_height;
        nonEmptyID++;
    }
    
    
}

// -----------------------------------------------------------------------------------------------------------------

void PrintLayoutNumeric::placeLinesInPage(LayoutPage& page, const int text_height, const float track_area_height,
                                          const int level_y_amount, const int pageHeight,
                                          const int x0, const int y0, const int x1)
{
    std::cout << "\n========\nplaceTracksInPage\n========\n";
    
    // ---- Lay out tracks
    float y_from = y0 + text_height*3;
    
    const int lineAmount = page.getLineCount();
    for(int l=0; l<lineAmount; l++)
    {
        std::cout << "\n====\nLine " << l << "\n====\n";
        
        
        //std::cout << "layoutLines[l].level_height = " << layoutLines[l].level_height << " track_area_height=" << track_area_height
        //          << " total_height=" << total_height << std::endl;
        
        // give a height proportional to its part of the total height
        float height = (track_area_height/level_y_amount) * page.getLine(l).level_height;
        
        float used_height = height;
        
        
        LayoutLine& line = page.getLine(l);
        
        // line too high, will look weird... shrink a bit
        while (used_height/(float)line.level_height > 100)
        {
            used_height *= 0.95;
        }
        
        // shrink total height when track is way too large (if page contains only a few tracks)
        if (height > pageHeight/5 && height > used_height*1.3) height = used_height*1.3;  
        
        
        float used_y_from = y_from;
        
        // center vertically in available space  if more space than needed
        if (used_height < height) used_y_from += (height - used_height)/2;
        
        //std::cout << "```` used_y_from=" << used_y_from << std::endl;
        
        // split margin above and below depending on position within page
        const int line_amount = page.getLineCount();
        const float position = line_amount == 0 ? 0 : float(l) / line_amount;
        int margin_above = 250*position;
        int margin_below = 250*(1-position);
        
        std::cout << "height=" << height << " used_height=" << used_height << " used_y_from=" << used_y_from << " margin_above=" << margin_above << " margin_below=" << margin_below << std::endl;
        
        this->divideLineAmongTracks(line, x0, used_y_from, x1, used_y_from+used_height, margin_below, margin_above);
        
        y_from += height;
        //std::cout << "yfrom is now " << y_from << std::endl;
    }
    
}

// -----------------------------------------------------------------------------------------------------------------