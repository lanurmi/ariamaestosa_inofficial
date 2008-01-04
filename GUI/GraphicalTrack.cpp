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

/*
 * The graphical part of a track (the data being held in Track)
 */

#include <iostream>
#include <wx/numdlg.h>

#include "OpenGL.h"

#include "Config.h"

#include "main.h"
#include "Pickers/InstrumentChoice.h"
#include "Pickers/DrumChoice.h"
#include "Pickers/MagneticGrid.h"
#include "Images/Drawable.h"
#include "Images/ImageProvider.h"
#include "Midi/Sequence.h"
#include "Midi/Track.h"
#include "Editors/KeyboardEditor.h"
#include "Editors/Editor.h"
#include "Editors/GuitarEditor.h"
#include "Editors/DrumEditor.h"
#include "Editors/RelativeXCoord.h"
#include "Editors/ControllerEditor.h"
#include "Editors/ScoreEditor.h"
#include "GUI/GLPane.h"
#include "GUI/MainFrame.h"
#include "GUI/GraphicalTrack.h"
#include "IO/IOUtils.h"

namespace AriaMaestosa {
	
const int EXPANDED_BAR_HEIGHT = 20;
const int COLLAPSED_BAR_HEIGHT = 5;

static int grid_x_begin=0, grid_x_end=0, track_name_x_begin=0, track_name_x_end=0;

GraphicalTrack::GraphicalTrack(Track* track, Sequence* seq)
{
	
	INIT_LEAK_CHECK();
	
    sequence = seq;
    GraphicalTrack::track = track;
    
    assert(getGLPane());
    assert(track);
    
    grid=new MagneticGrid(this);
    
    //mousex_current.setValue(0,WINDOW);
    lastMouseY=0;
    
    collapsed=false;
    dragging_resize=false;
    muted=false;
    docked = false;
    editorMode=KEYBOARD;
    
    height=128;
    
}

GraphicalTrack::~GraphicalTrack()
{
    delete keyboardEditor;
    delete guitarEditor;
	delete drumEditor;
	delete controllerEditor;
	
	delete grid;
	
	delete scoreEditor;
}


void GraphicalTrack::createEditors()
{
    
    keyboardEditor = new KeyboardEditor(track);
    guitarEditor = new GuitarEditor(track);
    drumEditor=new DrumEditor(track);
    controllerEditor=new ControllerEditor(track);
	
	scoreEditor=new ScoreEditor(track);
}

bool GraphicalTrack::mouseWheelMoved(int mx, int my, int value)
{
	if(my>from_y and my<to_y)
	{
		
		getCurrentEditor()->scroll( value / 75.0 );
		getGLPane()->render();
		
		return false; // event belongs to this track and was processed, stop searching
	}
	else
	{
		return true; // event does not belong to this track, continue searching
	}
}

// bool means:
//  true: the event does not belong to this track and so the program should continue searching to whom the event belongs.
//  false: the event belongs to this track and was processed
bool GraphicalTrack::processMouseClick(RelativeXCoord mousex, int mousey)
{
    
    dragging_resize=false;
    
	lastMouseY = mousey;
	
    if(mousey>from_y and mousey<to_y)
	{
        
	    sequence->setCurrentTrack(track);

        // resize drag
        if(mousey>to_y-15 and mousey<to_y-5)
		{
            dragging_resize=true;
        }
        
        if(!dragging_resize and !collapsed) getCurrentEditor()->mouseDown(mousex, mousey);
        
        if(!ImageProvider::imagesLoaded()) return true;
        assert(collapseDrawable->image!=NULL);
        assert(muteDrawable->image!=NULL);
		
        const int winX = mousex.getRelativeTo(WINDOW);
        // collapse
        if(winX > collapseDrawable->x and winX < collapseDrawable->x+collapseDrawable->getImageWidth() and
		   mousey > from_y+15 and mousey < from_y+35)
		{
            collapsed = !collapsed;
            getMainFrame()->updateVerticalScrollbar();
        }
        
        // dock
        if(winX > dockTrackDrawable->x and winX < dockTrackDrawable->x+muteDrawable->getImageWidth() and
		   mousey>from_y+10 and mousey<from_y+30)
		{
            docked = true;
            sequence->addToDock( this );
            getMainFrame()->updateVerticalScrollbar();
        }
        
        // mute
        if(winX > muteDrawable->x and winX < muteDrawable->x+muteDrawable->getImageWidth() and
		   mousey>from_y+10 and mousey<from_y+30)
		{
            muted = !muted;
            getMainFrame()->updateVerticalScrollbar();
        }
        
        // track name
        if(winX > track_name_x_begin and winX < track_name_x_end and
		   mousey > from_y+10 and mousey < from_y+30)
		{
            wxString msg=wxGetTextFromUser( _("Choose a new track title."), wxT("Aria Maestosa"), track->getName() );
            if(msg.Length()>0) track->setName( msg );
            getGLPane()->render();
        }
        
        // grid
        if(winX > grid_x_begin and winX < grid_x_end and mousey < from_y+30 and mousey > from_y+10)
		{
            getGLPane()->PopupMenu(grid, 220, from_y+30);
        }
        
        
        // instrument
        if(winX > getGLPane()->getWidth() - 160 and winX < getGLPane()->getWidth() - 160+14+116 and
		   mousey > from_y+10 and mousey < from_y+30)
		{
            if(editorMode==DRUM)
			{
				getMainFrame()->drumKit_picker->setParent(track);
				getGLPane()->PopupMenu((wxMenu*)(getMainFrame()->drumKit_picker), getGLPane()->getWidth() - 175, from_y+30);
			}
            else
			{
				getMainFrame()->instrument_picker->setParent(track);
				getGLPane()->PopupMenu((wxMenu*)(getMainFrame()->instrument_picker), getGLPane()->getWidth() - 175, from_y+30);
			}
        }
        
        // channel
		if(sequence->getChannelManagementType() == CHANNEL_MANUAL)
		{

			if(winX > getGLPane()->getWidth() - 185 and winX < getGLPane()->getWidth() - 185 + 14*2 and
			   mousey > from_y+10 and mousey < from_y+30)
			{
				const int channel = wxGetNumberFromUser( _("Enter the ID of the channel this track should play in"),
														 wxT(""),
														 _("Channel choice"),
														 track->getChannel(),
														 0,
														 15 );
				if(channel>=0 and channel<=15)
				{
					track->setChannel(channel);
					
					// check what is the isntrument currently used in thsi channel, if any
					const int trackAmount = sequence->getTrackAmount();
					for(int n=0; n<trackAmount; n++) // find another track that has same channel and use the same instrument
					{
						if( sequence->getTrack(n)->getChannel() == channel )
						{
							track->setInstrument(sequence->getTrack(n)->getInstrument(), true);
							break;
						}
					}//next
					
					getGLPane()->render();
				}
			}
		}			
		
        // modes
        if(mousey > from_y+10 and mousey < from_y+40)
		{

            if(winX > score_view->x and winX < score_view->x+30)
			{
                editorMode=SCORE;
            }
            else
			if(winX > keyboard_view->x and winX < keyboard_view->x+30)
			{
				// in midi, drums go to channel 9. So, if we exit drums, change channel so that it's not 9 anymore.
				if(editorMode == DRUM and sequence->getChannelManagementType() == CHANNEL_MANUAL) track->setChannel(0);
				
                editorMode=KEYBOARD;
            }
            else if(winX > guitar_view->x and winX < guitar_view->x+30)
			{
				// in midi, drums go to channel 9. So, if we exit drums, change channel so that it's not 9 anymore.
				if(editorMode == DRUM and sequence->getChannelManagementType() == CHANNEL_MANUAL) track->setChannel(0);
					
                editorMode=GUITAR;
                track->prepareNotesForGuitarEditor();
            }
            else if(winX > drum_view->x and winX < drum_view->x+30)
			{
				// in midi, drums go to channel 9 (10 if you start from one)
				if(sequence->getChannelManagementType() == CHANNEL_MANUAL) track->setChannel(9);
				
                editorMode=DRUM;
            }
            else if(winX > controller_view->x and winX < controller_view->x+30)
			{
                editorMode=CONTROLLER;
            }
            
        }
        
        return false;
    }
	else
	{
        return true;   
    }
}

bool GraphicalTrack::processRightMouseClick(RelativeXCoord x, int y)
{
    
	if(y>from_y and y<to_y)
	{
        
		getCurrentEditor()->rightClick(x,y);
		
		return false;
	}
	else
	{
		return true;   
	}
	
}

void GraphicalTrack::processMouseRelease()
{
	//std::cout << "mouse up GraphicalTrack" << std::endl;
	
    if(!dragging_resize) getCurrentEditor()->mouseUp(getGLPane()->getMouseX_current(), getGLPane()->getMouseY_current(),
													 getGLPane()->getMouseX_initial(), getGLPane()->getMouseY_initial());
    
    if(dragging_resize)
	{
        dragging_resize=false;
        getMainFrame()->updateVerticalScrollbar();   
    }
}

void GraphicalTrack::processMouseExited(RelativeXCoord x_now, int y_now, RelativeXCoord x_initial, int y_initial)
{
	getCurrentEditor()->mouseExited(x_now, y_now, x_initial, y_initial);
}

bool GraphicalTrack::processMouseDrag(RelativeXCoord x, int y)
{

    if((y>from_y and y<to_y) || dragging_resize)
	{
        
        /*
         *
         * until the end of the method, mousex_current/mousey_current contain the location of the mouse last time this event was thrown in the dragging process.
         * This can be used to determine the movement of the mouse.
         * At the end of the method, mousex_current/mousey_current are set to the current values.
         *
         */
        
        int barHeight=EXPANDED_BAR_HEIGHT;
        if(collapsed) barHeight=COLLAPSED_BAR_HEIGHT; 
		
        if(!dragging_resize) getCurrentEditor()->mouseDrag(x, y, getGLPane()->getMouseX_initial(), getGLPane()->getMouseY_initial());
        
        // resize drag
        if(dragging_resize)
		{
            
            if(height==35)
			{ // if it has reached minimal size, wait until mouse comes back over before resizing again
                if(y>to_y-15 and y<to_y-5 and (y-lastMouseY)>0 ) height+=(y-lastMouseY);
                
            }
			else
			{ // resize the track and check if it's not too small
                height+=(y-lastMouseY);
                if(height<35) height=35; //minimum size
            }
			getMainFrame()->updateVerticalScrollbar();
            
        }
        
        lastMouseY=y;
        
        
        return false;
        
    }
	else
	{
        return true;   
    }
}

void GraphicalTrack::setCollapsed(const bool collapsed)
{
    GraphicalTrack::collapsed = collapsed;
}
void GraphicalTrack::setHeight(const int height)
{
    GraphicalTrack::height = height;
}

int GraphicalTrack::getTotalHeight()
{

    if(docked) return 0;
    
    if(collapsed)
	{
        return 45; // COLLAPSED_BAR_HEIGHT
    }
	else
	{
        return EXPANDED_BAR_HEIGHT+50+height;
    }

}

int GraphicalTrack::getEditorHeight(){		return height;		}

void GraphicalTrack::renderHeader(const int x, const int y, const bool closed, const bool focus)
{
    
	const bool channel_mode = sequence->getChannelManagementType() == CHANNEL_MANUAL;
	
    int barHeight=EXPANDED_BAR_HEIGHT;
    if(closed) barHeight=COLLAPSED_BAR_HEIGHT;
    
    if(!focus) glColor3f(0.5, 0.5, 0.5);
    else glColor3f(1,1,1);
    
    glEnable(GL_TEXTURE_2D);
    
    // top left corner
    cornerDrawable->move(x+10,y);
    cornerDrawable->setFlip(false, false);
    cornerDrawable->render();
    
    // top border
    borderDrawable->move(x+30, y);
    borderDrawable->setFlip(false, false);
    borderDrawable->rotate(0);
    borderDrawable->scale((getGLPane()->getWidth() - 5 /*margin*/ -20 /*left round cornerDrawable*/ - 20 /*right round cornerDrawable*/)/20.0, 1 );
    borderDrawable->render();
    
    // top right corner
    cornerDrawable->move(x+ getGLPane()->getWidth() - 5 /* margin*/ -20 /*left round cornerDrawable*/, y);
    cornerDrawable->setFlip(true, false);
    cornerDrawable->render();
    
    // --------------------------------------------------
    
    // left border
    borderDrawable->move(x+ 30, y+20);
    borderDrawable->setFlip(false, true);
    borderDrawable->rotate(90);
    borderDrawable->scale(1, barHeight /*number of pixels high*/ /20.0 );
    borderDrawable->render();
    
    // right border
    borderDrawable->move(x+ getGLPane()->getWidth() - 5 /*margin*/ - 20 /*left round cornerDrawable*/ + 20 /*due to rotation of 90 degrees*/, y+20);
    borderDrawable->setFlip(false, false);
    borderDrawable->rotate(90);
    borderDrawable->scale(1, barHeight /*number of pixels high*/ /20.0 );
    borderDrawable->render();
    
    // --------------------------------------------------
    
    // center
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    
    if(!focus) glColor3f(0.31/2, 0.31/2, 0.31/2);
    else glColor3f(0.31, 0.31, 0.31);
    
    glBegin(GL_QUADS);
    glVertex2f(x+ 30, y+20);
    glVertex2f(x+ 30, y+20+barHeight);
    glVertex2f(x+ getGLPane()->getWidth() - 5 /* margin*/ - 20 /*right round cornerDrawable*/, y+20+barHeight);
    glVertex2f(x+ getGLPane()->getWidth() - 5 /* margin*/ - 20 /*right round cornerDrawable*/, y+20);
    glEnd();
    
    glEnable(GL_TEXTURE_2D);
    glColor3f(1,1,1);
    
    // --------------------------------------------------
    
    if(closed)
	{
        if(!focus) glColor3f(0.5, 0.5, 0.5);
        else glColor3f(1,1,1);
        
        // bottom left corner
        cornerDrawable->move(x+10, y+20+barHeight);
        cornerDrawable->setFlip(false, true);
        cornerDrawable->render();
        
        // bottom border
        borderDrawable->move(x+30, y+20+barHeight);
        borderDrawable->setFlip(false, true);
        borderDrawable->rotate(0);
        borderDrawable->scale((getGLPane()->getWidth() - 5 /*margin*/ -20 /*left round cornerDrawable*/ - 20 /*right round cornerDrawable*/)/20.0, 1 );
        borderDrawable->render();
        
        // bottom right corner
        cornerDrawable->move(x+ getGLPane()->getWidth() - 5 /*margin*/ -20 /*left round cornerDrawable*/, y+20+barHeight);
        cornerDrawable->setFlip(true, true);
        cornerDrawable->render();
        
        glColor4f(1,1,1,1);
        
    }
	else
	{
        
        // white area
        glLoadIdentity();
        
        if(editorMode != KEYBOARD) // keyboard editor draws its own backgound, so no need to draw it twice
		{
            glDisable(GL_TEXTURE_2D);
            
            glColor3f(1, 1, 1);
            
            glBegin(GL_QUADS);
            glVertex2f(x+10, y+barHeight+20);
            glVertex2f(x+10, y+barHeight+40+height);
            glVertex2f(x+getGLPane()->getWidth() - 5 , y+barHeight+40+height);
            glVertex2f(x+getGLPane()->getWidth() - 5 , y+barHeight+20);
            glEnd();
        }//end if
        
        
    }//end if
    
    glEnable(GL_TEXTURE_2D);
    
    glColor4f(1,1,1,1);
    
    if(!focus) glColor3f(0.5, 0.5, 0.5);
    else glColor3f(1,1,1);
    
    int draw_x = 20;
    
    // collapse
    if(collapsed) collapseDrawable->setImage( expandImg );
    else collapseDrawable->setImage( collapseImg );
    collapseDrawable->move(x+draw_x,y+15);
    draw_x += 28;
    collapseDrawable->render();
    
    // mute
    if(muted) muteDrawable->setImage(muteOnImg );
    else muteDrawable->setImage( muteOffImg );
    muteDrawable->move(x+draw_x,y+10);
    draw_x += 28;
    muteDrawable->render();
    
    // dock
    dockTrackDrawable->move(x+draw_x,y+11);
    draw_x += 20;
    dockTrackDrawable->render();
    
    // track name
    glLoadIdentity();
    
    track_name_x_begin = draw_x;
    
    comboBorderDrawable->move(x+draw_x,y+7);
    comboBorderDrawable->setFlip(false, false);
    comboBorderDrawable->render();
    draw_x+=14;
    
    comboBodyDrawable->move(x+draw_x, y+7);
    comboBodyDrawable->scale( 112 /*desired width*/ /4 , 1);
    comboBodyDrawable->render();
    draw_x+=112;
    
    comboBorderDrawable->move(x+draw_x, y+7 );
    comboBorderDrawable->setFlip(true,false);
    comboBorderDrawable->render();
    
    draw_x += 16;
    
    track_name_x_end = draw_x-8;
    
    glColor3f(0,0,0);
    glLoadIdentity();
    glRasterPos2f(x+track_name_x_begin+10 ,y+26);
    glDisable(GL_TEXTURE_2D);
    
	const unsigned int nameLen = track->getName().size();
	
    for(unsigned int i=0; i<nameLen; i++)
	{
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, track->getName()[i]);
        
        // if name is too long
        float rasterPos[4];
        glGetFloatv(GL_CURRENT_RASTER_POSITION,rasterPos);
        if(rasterPos[0] > x+draw_x - 25)
		{
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, '.');
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, '.');
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, '.');
            break;
        }
        
    }
    glEnable(GL_TEXTURE_2D);
    
    // grid
    if(!focus) glColor3f(0.5, 0.5, 0.5);
    else glColor3f(1,1,1);
    
    grid_x_begin = draw_x;
    
    comboBorderDrawable->move(x+draw_x,y+7);
    comboBorderDrawable->setFlip(false, false);
    comboBorderDrawable->render();
    draw_x += 14;
    
    comboBodyDrawable->move(x+draw_x, y+7);
    comboBodyDrawable->scale( 36 /*desired width*/ /4 , 1);
    comboBodyDrawable->render();
    draw_x += 36;
    
    comboSelectDrawable->move(x+draw_x, y+7 );
    comboSelectDrawable->render();
    
    grid_x_end = draw_x+25;
    
    glLoadIdentity();
    glDisable(GL_TEXTURE_2D);
    
    glColor3f(0,0,0);
    glRasterPos2f(comboBorderDrawable->x + 10,y+26);
    draw_x += 41;
    
    for(int i=0; grid->label[i]; i++)
	{
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, grid->label[i]);
    }
    
    
    // view mode
    glColor4f(1,1,1,1);
    
    glEnable(GL_TEXTURE_2D);
    
    
    if(editorMode==SCORE and focus) glColor3f(1,1,1);
    else glColor3f(0.4, 0.4, 0.4);
    score_view->move(x+draw_x, y+7);
    score_view->render();
    draw_x+=32;
    
    if(editorMode==KEYBOARD and focus) glColor3f(1,1,1);
    else glColor3f(0.4, 0.4, 0.4);
    keyboard_view->move(x+draw_x, y+7);
    keyboard_view->render();
    draw_x += 32;
    
    if(editorMode==GUITAR and focus) glColor3f(1,1,1);
    else glColor3f(0.4, 0.4, 0.4);
    guitar_view->move(x+draw_x, y+7);
    guitar_view->render();
    draw_x += 32;
    
    if(editorMode==DRUM and focus) glColor3f(1,1,1);
    else glColor3f(0.4, 0.4, 0.4);
    drum_view->move(x+draw_x, y+7);
    drum_view->render();
    draw_x += 32;
    
    if(editorMode==CONTROLLER and focus) glColor3f(1,1,1);
    else glColor3f(0.4, 0.4, 0.4);
    controller_view->move(x+draw_x, y+7);
    controller_view->render();
    draw_x += 32;
    
    // ------------------------------- instrument name ---------------------------
    if(!focus) glColor3f(0.5, 0.5, 0.5);
    else glColor3f(1,1,1);
    glLoadIdentity();
    
    // draw box
    comboBorderDrawable->move(x+getGLPane()->getWidth() - 160 ,y+7);
    comboBorderDrawable->setFlip(false, false);
    comboBorderDrawable->render();
    
    comboBodyDrawable->move(x+getGLPane()->getWidth() - 160+14, y+7);
    comboBodyDrawable->scale(116 /*desired width*/ /4 , 1);
    comboBodyDrawable->render();
    
    comboBorderDrawable->move(x+getGLPane()->getWidth() - 160+14+116, y+7 );
    comboBorderDrawable->setFlip(true,false);
    comboBorderDrawable->render();
    
    // get instrument name to display    
    std::string instrumentname;
    if(editorMode == DRUM) instrumentname = getMainFrame()->drumKit_picker->getDrumName( track->getDrumKit() );
    else instrumentname = getMainFrame()->instrument_picker->getInstrumentName( track->getInstrument() );
    
    // draw instrument name
    glColor3f(0,0,0);
    glLoadIdentity();
    glRasterPos2f(x+getGLPane()->getWidth() - 155+3,y+26);
    glDisable(GL_TEXTURE_2D);
    
    for(unsigned int i=0; i<instrumentname.size(); i++)
	{
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, instrumentname[i]);
    }
	
	// --------------- channel choice ------------
	if(channel_mode)
	{
		if(!focus) glColor3f(0.5, 0.5, 0.5);
		else glColor3f(1,1,1);
		glLoadIdentity();
		glEnable(GL_TEXTURE_2D);
		
		// draw box
		comboBorderDrawable->move(x+getGLPane()->getWidth() - 185 ,y+7);
		comboBorderDrawable->setFlip(false, false);
		comboBorderDrawable->render();
		
		comboBorderDrawable->move(x+getGLPane()->getWidth() - 185+14, y+7 );
		comboBorderDrawable->setFlip(true,false);
		comboBorderDrawable->render();
		
		// draw channel number
		char buffer[3];
		sprintf ( buffer, "%d", track->getChannel() );
		std::string channelName = buffer;
		
		glColor3f(0,0,0);
		glLoadIdentity();
		
		const int char_amount_in_channel_name = channelName.size();
		if(char_amount_in_channel_name == 1) glRasterPos2f(x+getGLPane()->getWidth() - 185 + 10,y+26);
		else glRasterPos2f(x+getGLPane()->getWidth() - 185 + 7,y+26);
			
		glDisable(GL_TEXTURE_2D);
		
		for(int i=0; i<char_amount_in_channel_name; i++)
		{
			glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, channelName[i]);
		}
	}
	
}// end func

int GraphicalTrack::render(const int y, const int currentTick, const bool focus)
{
    
    if(!ImageProvider::imagesLoaded()) return 0;
    
    // docked tracks are not drawn
    if(docked)
	{
        from_y=-1;
        to_y=-1;
        return y;
    }
    
    int barHeight=EXPANDED_BAR_HEIGHT;
    if(collapsed) barHeight=COLLAPSED_BAR_HEIGHT;

    from_y=y;
    
    if(collapsed) to_y = from_y + 45;
    else to_y = y+barHeight+50+height;
    
    assert(getGLPane());


	// tell the editor about its new location
    getCurrentEditor()->updatePosition(from_y, to_y, getGLPane()->getWidth(), height, barHeight);

	// don't waste time drawing it if out of bounds
	if(to_y < 0) return to_y;
	if(from_y > getGLPane()->getHeight()) return to_y;
	
    renderHeader(0, y, collapsed, focus);

    if(!collapsed)
	{
        // --------------------------------------------------
        // render editor
        getCurrentEditor()->render(getGLPane()->getMouseX_current(),
								   getGLPane()->getMouseY_current(),
								   getGLPane()->getMouseX_initial(),
								   getGLPane()->getMouseY_initial(), focus);
        // --------------------------------------------------
        // render playback progress line
    
        glLoadIdentity();
        glDisable(GL_TEXTURE_2D);
        
        if( currentTick!=-1 and not getGLPane()->leftArrow and not getGLPane()->rightArrow)
		{
            glColor3f(0.8, 0, 0);
            
            RelativeXCoord tick(currentTick, MIDI);
            const int x_coord = tick.getRelativeTo(WINDOW);
            
            glLineWidth(1);
            
            glBegin(GL_LINES);
            glVertex2f(x_coord, getCurrentEditor()->getEditorYStart());
            glVertex2f(x_coord, getCurrentEditor()->getYEnd());
            glEnd();
            
        }
        glEnable(GL_TEXTURE_2D);
        
        // --------------------------------------------------
        // render track borders
        
        if(!focus) glColor3f(0.5, 0.5, 0.5);
        else glColor3f(1,1,1);
        
        // bottom left corner
        whiteCornerDrawable->move(10,y+20+barHeight+height);
        whiteCornerDrawable->setFlip(false, false);
        whiteCornerDrawable->render();
        
        // bottom border
        whiteBorderDrawable->move(30, y+20+barHeight+height);
        whiteBorderDrawable->setFlip(false, false);
        whiteBorderDrawable->rotate(0);
        whiteBorderDrawable->scale((getGLPane()->getWidth() - 5 /* margin*/ - 20 /*left round cornerDrawable*/ - 20 /*right round cornerDrawable*/)/20.0, 1 );
        whiteBorderDrawable->render();
        
        // bottom right corner
        whiteCornerDrawable->move(getGLPane()->getWidth() - 5 /* margin*/ - 20 /*left round cornerDrawable*/, y+20+barHeight+height);
        whiteCornerDrawable->setFlip(true, false);
        whiteCornerDrawable->render();
        
        // --------------------------------------------------
        
        // left borderDrawable
        whiteBorderDrawable->move(30, y+barHeight+20);
        whiteBorderDrawable->setFlip(false, false);
        whiteBorderDrawable->rotate(90);
        whiteBorderDrawable->scale(1, height /*number of pixels high*/ /20.0 );
        whiteBorderDrawable->render();
        
        // right borderDrawable
        whiteBorderDrawable->move(getGLPane()->getWidth() - 5 , y+barHeight+20);
        whiteBorderDrawable->setFlip(false, true);
        whiteBorderDrawable->rotate(90);
        whiteBorderDrawable->scale(1, height /*number of pixels high*/ /20.0 );
        whiteBorderDrawable->render();
        
    }
    
    glEnable(GL_TEXTURE_2D);
    
    // done
    return to_y;
    
}

Editor* GraphicalTrack::getCurrentEditor()
{
    if(editorMode==KEYBOARD) return keyboardEditor;
    else if(editorMode==GUITAR) return guitarEditor;
    else if(editorMode==DRUM) return drumEditor;
    else if(editorMode==CONTROLLER) return controllerEditor;
	else if(editorMode==SCORE) return scoreEditor;
    else
	{
        std::cout << "No such editor!" << std::endl;
        assert(false);
		return NULL; // shut up warnings
    }
}

void GraphicalTrack::setEditorMode(int mode)
{
    editorMode = mode;
}

// ---------------------------------------- serialization -----------------------------------------
void GraphicalTrack::saveToFile(wxFileOutputStream& fileout)
{
	
	writeData( wxT("<editor mode=\"") + to_wxString(editorMode) +
			   wxT("\" height=\"") + to_wxString(height) +
			   wxT("\" collapsed=\"") + (collapsed?wxT("true"):wxT("false")) +
			   wxT("\" muted=\"") + (muted?wxT("true"):wxT("false")) +
			   wxT("\"/>\n")
			   , fileout );
	
	grid->saveToFile(fileout);
    //keyboardEditor->instrument->saveToFile(fileout);
    //drumEditor->drumKit->saveToFile(fileout);
	
	writeData( wxT("<instrument id=\"") + to_wxString( track->getInstrument() ) + wxT("\"/>\n"), fileout);
	writeData( wxT("<drumkit id=\"") + to_wxString( track->getDrumKit() ) + wxT("\"/>\n"), fileout);
	
    writeData( wxT("<key sharps=\"") + to_wxString( scoreEditor->getKeySharpsAmount() ) +
               wxT("\" flats=\"") + to_wxString( scoreEditor->getKeyFlatsAmount() ) +
               + wxT("\"/>\n"), fileout);
    
	// guitar tuning
	writeData( wxT("<guitartuning "), fileout);
    for(unsigned int n=0; n<guitarEditor->tuning.size(); n++)
	{
        writeData( wxT(" string")+ to_wxString((int)n) + wxT("=\"") + to_wxString((int)guitarEditor->tuning[n]) + wxT("\""), fileout );   
    }
	writeData( wxT("/>\n\n"), fileout);

}

bool GraphicalTrack::readFromFile(irr::io::IrrXMLReader* xml)
{

	if (!strcmp("editor", xml->getNodeName()))
	{

		const char* mode_c = xml->getAttributeValue("mode");
		if( mode_c != NULL ) editorMode = atoi( mode_c );
		else
		{
			editorMode = KEYBOARD;
			std::cout << "Missing info from file: editor mode" << std::endl;
		}
		
		const char* height_c = xml->getAttributeValue("height");
		if( height_c != NULL ) height = atoi( height_c );
		else
		{
			std::cout << "Missing info from file: track height" << std::endl;
			height = 200;	
		}
		
		const char* collapsed_c = xml->getAttributeValue("collapsed");
		if( collapsed_c != NULL )
		{
			if(!strcmp(collapsed_c, "true")) collapsed = true;
			else if(!strcmp(collapsed_c, "false")) collapsed = false;
			else
			{
				std::cout << "Unknown keyword for attribute 'collapsed' in track: " << collapsed_c << std::endl;
				collapsed = false;
			}
			
		}
		else
		{
			std::cout << "Missing info from file: track collapsed" << std::endl;
			collapsed = false;
		}
		
		const char* muted_c = xml->getAttributeValue("muted");
		if( muted_c != NULL )
		{
			if(!strcmp(muted_c, "true")) muted = true;
			else if(!strcmp(muted_c, "false")) muted = false;
			else
			{
				muted = false;
				std::cout << "Unknown keyword for attribute 'muted' in track: " << muted_c << std::endl;
			}
			
		}
		else
		{
			std::cout << "Missing info from file: track muted" << std::endl;
			muted = false;
		}
		
		
	}
	else if (!strcmp("magneticgrid", xml->getNodeName()))
	{

		if(! grid->readFromFile(xml) )
			return false;
		
	}
    else if (!strcmp("key", xml->getNodeName()))
	{
        std::cout << "Found 'key'!" << std::endl;
        char* flats_c = (char*)xml->getAttributeValue("flats");
        char* sharps_c = (char*)xml->getAttributeValue("sharps");
        
        int sharps = 0, flats = 0;
        if(flats_c == NULL and sharps_c == NULL);
        else
        {
            if(flats_c != NULL) flats = atoi(flats_c);
            if(sharps_c != NULL) sharps = atoi(sharps_c);
            
            std::cout << "sharps = " << sharps << " flats = " << flats << std::endl;
            
            if(sharps > flats) scoreEditor->loadKey(SHARP, sharps);
            else scoreEditor->loadKey(FLAT, flats);
        }
        
    }
	else if (!strcmp("guitartuning", xml->getNodeName()))
	{
		
		guitarEditor->tuning.clear();
		
		int n=0;
		char* string_v = (char*)xml->getAttributeValue("string0");
		
		while(string_v != NULL)
		{
			guitarEditor->tuning.push_back( atoi(string_v) );

			n++;
			wxString tmp = wxT("string") + to_wxString(n);
			string_v = (char*)xml->getAttributeValue( toCString(tmp) );
		}
		
		if(guitarEditor->tuning.size() < 3)
		{
			std::cout << "FATAL ERROR: Invalid tuning!! only " << guitarEditor->tuning.size() << " strings found" << std::endl;	
			return false;
		}
		
		guitarEditor->tuningUpdated(false);
	} 
	
	return true;
	
}

}
