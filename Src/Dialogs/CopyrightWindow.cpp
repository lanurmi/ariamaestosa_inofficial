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



#include "Dialogs/CopyrightWindow.h"
#include "GUI/MainFrame.h"
#include "Midi/Sequence.h"
#include "Midi/CommonMidiUtils.h"

#include <iostream>

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/radiobox.h>
#include <wx/spinctrl.h>

#include "Utils.h"




namespace AriaMaestosa
{
    

/**
  * @ingroup dialogs
  * @brief the dialog where sequence info can be edited (title, author, etc...)
  * @note this is a private class, it won't be instanciated directly
  * @see CopyrightWindow::show
  * @see CopyrightWindow::hide
  * @see CopyrightWindow::free
  */
class CopyrightWindowClass : public wxDialog
{
    long ID_KEY_SIG_RADIOBOX;
    
    wxBoxSizer* m_boxSizer;
    wxTextCtrl* m_copyrightInput;
    wxTextCtrl* m_nameInput;
    wxSpinCtrl* m_keySymbolAmountSpinCtrl;
    wxRadioBox* m_keySigRadioBox;
    wxStaticText* m_songLength;
    wxButton* m_okBtn;
    wxButton* m_cancelBtn;
    
    Sequence* m_sequence;
    int m_code;

public:
    LEAK_CHECK();
    
    
    CopyrightWindowClass(Sequence* seq) : wxDialog(getMainFrame(), wxID_ANY,
                //I18N: - title of the copyright/info dialog
                _("Copyright and song info"),
                wxDefaultPosition, wxSize(400,400), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER )
    {
        m_sequence = seq;
        
        ID_KEY_SIG_RADIOBOX = wxNewId();

        m_boxSizer=new wxBoxSizer(wxVERTICAL);

        //I18N: - title of the copyright/info dialog
        m_boxSizer->Add( new wxStaticText(this, wxID_ANY,  _("Song name") ) , 0, wxLEFT|wxTOP|wxEXPAND, 10 );

        // song name
        wxSize size_horz = wxDefaultSize;
        size_horz.x = 400;
        m_nameInput = new wxTextCtrl( this, wxID_ANY,  m_sequence->getInternalName(), wxDefaultPosition, size_horz);
        m_boxSizer->Add( m_nameInput, 0, wxALL|wxEXPAND, 10 );

        //I18N: - title of the copyright/info dialog
        m_boxSizer->Add( new wxStaticText(this, wxID_ANY,  _("Copyright") ) , 0, wxLEFT|wxTOP|wxEXPAND, 10  );

        // text area
        m_copyrightInput = new wxTextCtrl( this, wxID_ANY, m_sequence->getCopyright(), wxDefaultPosition, wxSize(400,75), wxTE_MULTILINE );
        m_boxSizer->Add( m_copyrightInput, 1, wxALL|wxEXPAND, 10 );


        //I18N: - default key signature
        m_boxSizer->Add( new wxStaticText(this, wxID_ANY,  _("Default key signature") ) ,
                      0, wxLEFT|wxTOP|wxEXPAND, 10 );

        wxBoxSizer* keySymbolAmountBoxSizer = new wxBoxSizer(wxHORIZONTAL);
        wxString symbolTypeChoices[3] =
        {
            _("None"),
            _("Sharps"),
            _("Flats")
        };
        
        m_keySigRadioBox = new wxRadioBox(this, ID_KEY_SIG_RADIOBOX, _("Symbols"), wxDefaultPosition, wxDefaultSize, 3, symbolTypeChoices, 1);
        Connect(ID_KEY_SIG_RADIOBOX,wxEVT_COMMAND_RADIOBOX_SELECTED,(wxObjectEventFunction)&CopyrightWindowClass::OnKeySigRadioBoxSelect);
 
        keySymbolAmountBoxSizer->Add(m_keySigRadioBox, 1, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
        
        m_keySymbolAmountSpinCtrl = new wxSpinCtrl(this, wxID_ANY, _T("0"), wxDefaultPosition, wxSize(38,-1), 0, 0, 7, 0);
        m_keySymbolAmountSpinCtrl->SetValue(_T("0"));
        keySymbolAmountBoxSizer->Add(m_keySymbolAmountSpinCtrl, 0, wxALL|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);

        m_boxSizer->Add(keySymbolAmountBoxSizer, 1, wxALL|wxEXPAND|wxALIGN_CENTER_HORIZONTAL|wxALIGN_CENTER_VERTICAL, 5);
    
        // song length
        m_songLength = new wxStaticText(this, wxID_ANY, wxString(_("Song duration :"))+wxT(" ??:??"),
                                      wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
        m_boxSizer->Add( m_songLength, 0, wxLEFT|wxTOP|wxEXPAND, 10 );
    
        // ok button
        m_okBtn = new wxButton( this, wxID_OK, _("OK"));
        m_okBtn->SetDefault();
        m_boxSizer->Add( m_okBtn, 0, wxALL, 0 );

        // ok button
        m_cancelBtn = new wxButton( this, wxID_CANCEL, _("Cancel"));
        m_cancelBtn->SetDefault();
        m_boxSizer->Add( m_cancelBtn, 0, wxALL, 0 );

        wxStdDialogButtonSizer* stdDialogButtonSizer = new wxStdDialogButtonSizer();
        stdDialogButtonSizer->AddButton(m_okBtn);
        stdDialogButtonSizer->AddButton(m_cancelBtn);
        stdDialogButtonSizer->Realize();
        m_boxSizer->Add(stdDialogButtonSizer, 0, wxALL|wxEXPAND, 5);

        SetSizer( m_boxSizer );
        m_boxSizer->Layout();
        m_boxSizer->SetSizeHints( this );

        // find song duration
        int last_tick = -1;
        const int track_amount = seq->getTrackAmount();
        for (int n=0; n<track_amount; n++)
        {
            const int duration = seq->getTrack(n)->getDuration();
            if (duration > last_tick) last_tick = duration;
        }

        int song_duration = getTimeAtTick(last_tick, seq);

        wxString duration_label = wxString::Format(wxString(_("Song duration :")) + wxT("  %i:%.2i"), (int)(song_duration/60), song_duration%60);
        //std::cout << song_duration << " --> " << duration_label.mb_str() << std::endl;
        m_songLength->SetLabel(duration_label);
    }


    void OnKeySigRadioBoxSelect(wxCommandEvent& event)
    {
        int selection;
        
        selection = event.GetSelection();
        m_keySymbolAmountSpinCtrl->Enable(selection>0);
        m_keySymbolAmountSpinCtrl->SetValue((selection==0) ? 0 : 1);
    }


    void okClicked(wxCommandEvent& evt)
    {
        KeyType keyType;
        int selection;
        
        m_sequence->setCopyright( m_copyrightInput->GetValue() );
        m_sequence->setInternalName( m_nameInput->GetValue() );
        
        selection = m_keySigRadioBox->GetSelection();
        
        switch (selection)
        {
            case 0 : keyType = KEY_TYPE_C; break;
            case 1 : keyType = KEY_TYPE_SHARPS; break;
            case 2 : keyType = KEY_TYPE_FLATS; break;
            // what to do with KEY_TYPE_CUSTOM?
            default : keyType = KEY_TYPE_C; break;
            
        }
        m_sequence->setDefaultKeyType(keyType);
        m_sequence->setDefaultKeySymbolAmount(m_keySymbolAmountSpinCtrl->GetValue());

        CopyrightWindow::hide();
    }

    void show()
    {
        KeyType keyType;
        int selection;
        
        m_nameInput->SetValue( m_sequence->getInternalName() );
        m_copyrightInput->SetValue( m_sequence->getCopyright() );
        
        keyType = m_sequence->getDefaultKeyType();
        switch (keyType)
        {
            case KEY_TYPE_C : selection = 0; break;
            case KEY_TYPE_SHARPS : selection = 1; break;
            case KEY_TYPE_FLATS : selection = 2; break;
            // what to do with KEY_TYPE_CUSTOM?
            default : selection = 0; break;
            
        }
        m_keySigRadioBox->SetSelection(selection);

        if (keyType==KEY_TYPE_C)
        {
            m_keySymbolAmountSpinCtrl->Enable(false);
        }
        else
        {
            m_keySymbolAmountSpinCtrl->SetValue(m_sequence->getDefaultKeySymbolAmount());
        }

        wxDialog::Center();
        m_code = wxDialog::ShowModal();
    }


    void hide()
    {
        wxDialog::EndModal(m_code);
        //wxDialog::Hide();
    }

    void onCancel(wxCommandEvent& evt)
    {
        CopyrightWindow::hide();
    }

    DECLARE_EVENT_TABLE();

};

BEGIN_EVENT_TABLE( CopyrightWindowClass, wxDialog )

    EVT_BUTTON( wxID_OK, CopyrightWindowClass::okClicked )
    EVT_BUTTON( wxID_CANCEL, CopyrightWindowClass::onCancel )
    EVT_COMMAND(wxID_CANCEL, wxEVT_COMMAND_BUTTON_CLICKED, CopyrightWindowClass::onCancel )

END_EVENT_TABLE()


namespace CopyrightWindow
{

CopyrightWindowClass* copyrightWindow=NULL;

void show(Sequence* seq)
{
    copyrightWindow = new CopyrightWindowClass(seq);
    copyrightWindow->show();
}

void hide()
{
    copyrightWindow->hide();
    copyrightWindow->Destroy();
}

void free()
{
}

}

}

