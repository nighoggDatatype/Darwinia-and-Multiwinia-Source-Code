#include "lib/universal_include.h"
#include "lib/debug_utils.h"
#include "lib/text_renderer.h"
//#include "lib/input.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/resource.h"

#include "lib/input/input.h"
#include "lib/targetcursor.h"

#include "worldobject/entity.h"
#include "worldobject/building.h"

#include "interface/drop_down_menu.h"
#include "interface/input_field.h"
#include "interface/filedialog.h"
#include "interface/message_dialog.h"
#include "interface/scrollbar.h"
#include "interface/soundeditor_window.h"
#include "interface/soundparameter_window.h"

#include "sound/soundsystem.h"
#include "sound/sound_library_3d.h"

#include "app.h"
#include "camera.h"
#include "location.h"
#include "main.h"
#include "renderer.h"


#ifdef SOUND_EDITOR


class SoundEventButton : public DarwiniaButton
{
public:
    int m_eventIndex;

public:
    SoundEventButton(): DarwiniaButton()
    {
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        SoundSourceBlueprint *ssb = sew->GetSoundSourceBlueprint();

        int actualIndex = m_eventIndex;
        if( sew->m_eventScrollbar ) actualIndex += sew->m_eventScrollbar->m_currentValue;

        if( ssb && ssb->m_events.ValidIndex(actualIndex) )
        {
            SoundEventBlueprint *seb = ssb->m_events[actualIndex];
            SetCaption( seb->m_eventName );

            if( sew->m_eventIndex == actualIndex )
            {
                DarwiniaButton::Render( realX, realY, true, clicked );
            }
            else
            {
                DarwiniaButton::Render( realX, realY, highlighted, clicked );
            }
        }
    }

    void MouseUp()
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;        

        int actualIndex = m_eventIndex;
        if( sew->m_eventScrollbar ) actualIndex += sew->m_eventScrollbar->m_currentValue;

        sew->SelectEvent( actualIndex );
    }
};



class ObjectTypeMenu : public DropDownMenu
{
public:
    char m_objectGroup[64];

public:
	ObjectTypeMenu()
	:	DropDownMenu(true)
	{
	}

    void SelectOption( int _option )
    {
        DropDownMenu::SelectOption( _option );

        if( _option != -1 )
        {
            SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
            sew->SelectEvent( -1 );
            
            EclButton *button = m_parent->GetButton( "New Event" );
            AppDebugAssert( button );
        
            EclButton *selectButton = m_parent->GetButton( "Select Object" );
            AppAssert( selectButton );

            //
            // Remove existing event buttons

            int i = 0;
            while( true )
            {
                char name[64];
                sprintf( name, "Event %d", i );
                if( !m_parent->GetButton( name ) )
                {
                    break;
                }

                m_parent->RemoveButton( name );
                ++i;
            }

            if( sew->m_eventScrollbar )
            {
                sew->m_eventScrollbar->Remove();
                delete sew->m_eventScrollbar;
                sew->m_eventScrollbar = NULL;
            }

            //
            // Create the existing event buttons

            SoundSourceBlueprint *ssb = sew->GetSoundSourceBlueprint();
            int numExistingEvents = ssb->m_events.Size();
            numExistingEvents++;

            float availableSpace = ( selectButton->m_y - 200 );
            float height = 16;
            float gap = 3;
            float width = 150;
            int maxEvents = availableSpace / ( height + gap );

            if( maxEvents < numExistingEvents )
            {
                sew->m_eventScrollbar = new ScrollBar( m_parent );
                sew->m_eventScrollbar->Create( "EventScrollbar", 147, 190, 13, maxEvents*(height+gap), numExistingEvents + 10, maxEvents );
                width = 135;
            }

            for( int i = 0; i < maxEvents; ++i )
            {
                char name[64];
                sprintf( name, "Event %d", i );
                SoundEventButton *event = new SoundEventButton();
                event->SetShortProperties( name, 10, 190 + i * (height+gap), width, height, UnicodeString(name) );
                event->m_eventIndex = i;
                m_parent->RegisterButton( event );
            }    


            //
            // Fill the event menu

            DropDownMenu *menu = (DropDownMenu *) button;
            menu->Empty();
            
            if (stricmp(m_objectGroup, "Entity") == 0)
            {
                Entity *entity = Entity::NewEntity( _option );
                LList<char *> events;
                entity->ListSoundEvents( &events ); // ListSoundEvents only adds pointers to static strings, so no need for EmptyAndDelete
                delete entity;
                for( int i = 0; i < events.Size(); ++i )
                {
                    menu->AddOption( events[i] );
                }
            }
			else if (stricmp(m_objectGroup, "Building") == 0)
			{
                Building *building = Building::CreateBuilding( _option );
                LList<char *> events;
                building->ListSoundEvents( &events );
                delete building;
                for( int i = 0; i < events.Size(); ++i )
                {
                    menu->AddOption( events[i] );
                }
            }
			else
	        {
                LList<char *> events;
                SoundSourceBlueprint::ListSoundEvents( _option, &events );
                for( int i = 0; i < events.Size(); ++i )
                {
                    menu->AddOption( events[i] );
                }
            }
        }
    }
};


class ObjectGroupMenu : public DropDownMenu
{
public:
	ObjectGroupMenu()
	:	DropDownMenu(true)
	{
	}

    void SelectOption( int _value )
    {
        DropDownMenu::SelectOption( _value );
		int optionIndex = FindValue(_value);

        if( optionIndex != -1 )
        {
            SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
            sew->SelectEvent( -1 );

            EclButton *button = m_parent->GetButton( "[ObjectType]" );
            AppDebugAssert( button );
        
            ObjectTypeMenu *menu = (ObjectTypeMenu *) button;
            menu->Empty();
            strncpy(menu->m_objectGroup, m_options[optionIndex]->m_word, sizeof(menu->m_objectGroup) - 1);
    
            EclButton *newEventButton = m_parent->GetButton( "New Event" );
            AppDebugAssert( newEventButton );        
            DropDownMenu *newEventMenu = (DropDownMenu *) newEventButton;
            newEventMenu->Empty();
            
            if (stricmp(m_options[optionIndex]->m_word, "Entity") == 0)
            {
                for( int i = 0; i < Entity::NumEntityTypes; ++i )
                {
                    menu->AddOption( Entity::GetTypeName(i) );
                }
			}
			else if (stricmp(m_options[optionIndex]->m_word, "Building") == 0)
			{
                for( int i = 0; i < Building::NumBuildingTypes; ++i )
                {
                    menu->AddOption( Building::GetTypeName(i) );
                }
			}
			else
			{
				for( int i = 0; i < SoundSourceBlueprint::NumOtherSoundSources; ++i )
                {
                    menu->AddOption( SoundSourceBlueprint::GetSoundSourceName(i) );
                }
            }
        }
    }
};


class NewSoundEventMenu : public DropDownMenu
{
public:
    void SelectOption( int _value )
    {
        DropDownMenu::SelectOption( _value );
		int optionIndex = FindValue(_value);

        if( optionIndex != -1 )
        {
            SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
            SoundSourceBlueprint *ssb = sew->GetSoundSourceBlueprint();
            if( ssb )
            {
				char eventName[128];
				sew->GetSelectionName(eventName);
				strcat(eventName, " ");
				strcat(eventName, m_options[optionIndex]->m_word);

                SoundEventBlueprint *event = new SoundEventBlueprint();
                event->SetEventName( m_options[optionIndex]->m_word );
                event->m_instance = new SoundInstance();
				event->m_instance->SetEventName(eventName, m_options[optionIndex]->m_word);
                ssb->m_events.PutDataAtEnd( event );
                int eventIndex = ssb->m_events.Size() - 1;
                sew->SelectEvent( eventIndex );
            }
            SelectOption( -1 );
        }
    }
};


class SoundPositionButton : public DarwiniaButton
{
public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        DarwiniaButton::Render( realX, realY, false, false );
        
        //
        // Render guidelines

        glColor4f( 1.0, 1.0, 1.0, 0.5 );
        glBegin( GL_LINES );
            glVertex2i( realX + 10, realY + m_h/2 );
            glVertex2i( realX + m_w - 10, realY + m_h/2 );

            glVertex2i( realX + m_w/2, realY + 10 );
            glVertex2i( realX + m_w/2, realY + m_h - 10 );
        glEnd();

        
        //
        // Render sound position

        Vector3 soundPos = g_app->m_soundSystem->m_editorPos;

        float midX = realX + m_w/2.0;
        float midY = realY + m_h/2.0;

        float scale = 5.0;
        float size = 5.0;

        glColor4f( 1.0, 1.0, 1.0, 1.0 );
        glBegin( GL_LINE_LOOP );
            glVertex2f( midX + soundPos.x/scale - size, midY + soundPos.z/scale - size );
            glVertex2f( midX + soundPos.x/scale + size, midY + soundPos.z/scale - size );
            glVertex2f( midX + soundPos.x/scale + size, midY + soundPos.z/scale + size );
            glVertex2f( midX + soundPos.x/scale - size, midY + soundPos.z/scale + size );
        glEnd();


        //
        // Handle mouse input

        float posX = soundPos.x;
        float posZ = soundPos.z;

		bool mousePressed = g_inputManager->controlEvent( ControlEclipseLMousePressed );
        if( mousePressed && EclMouseInButton(m_parent,this) )                    
        {
            posX = ( g_target->X() - midX ) * 5.0;
            posZ = ( g_target->Y() - midY ) * 5.0;
        }


        //  BUGFIX
        //  In the sound editor, when make a sample play looped, then stop it and then 
        //  click play or looped again no sound comes out. 
        //  This only occurs if the position is exactly on the camera
        if( posX == 0.0 && posZ == 0.0 ) posZ = 0.01;

        g_app->m_soundSystem->m_editorPos.Set( posX, 0.01, posZ );
    }
};


class PlayButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        sew->StartPlayback(false);
    }
};


class PlayRepeatButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        sew->StartPlayback(true);
    }
};


class StopButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        sew->StopPlayback();
    }
};


class SelectObjectButton: public DarwiniaButton
{
public:
	void MouseUp()
	{
		SoundEditorWindow *sew = (SoundEditorWindow *)m_parent;
		sew->m_objectSelectorEnabled = !sew->m_objectSelectorEnabled;
	}
};


class SaveAllButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        g_app->m_soundSystem->SaveBlueprints();
    }
};


class DeleteEventButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        SoundSourceBlueprint *ssb = sew->GetSoundSourceBlueprint();
        if( ssb->m_events.ValidIndex( sew->m_eventIndex ) )
        {
            sew->StopPlayback();
            SoundEventBlueprint *seb = ssb->m_events[ sew->m_eventIndex ];
            ssb->m_events.RemoveData( sew->m_eventIndex );
            delete seb;
            sew->SelectEvent( -1 );
        }
    }
};


class SoundSelector : public FileDialog
{
public:
    SoundSelector( char *_name, char *_parent, char *_path, char *_filter )
        :   FileDialog( _name, _parent, _path, _filter )
    {
    }

    void FileSelected( char *_filename )
    {
        char const *extensionRemoved = RemoveExtension( _filename );
		char const *err = g_app->m_soundSystem->IsSoundSourceOK(extensionRemoved);
		if (err)
		{
			MessageDialog *win = new MessageDialog("Alert", err);
			EclRegisterWindow(win);
			return;
		}

        EclWindow *window = EclGetWindow( m_parent );
        SoundEditorWindow *sew = (SoundEditorWindow *) window;
        if( sew )
        {
            SoundEventBlueprint *seb = sew->GetSoundEventBlueprint();
            seb->m_instance->SetSoundName( extensionRemoved );
        }
    }
};


class SoundSelectorButton : public DarwiniaButton
{
public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        DarwiniaButton::Render( realX, realY, highlighted, clicked );

        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        if( sew )
        {
            SoundEventBlueprint *seb = sew->GetSoundEventBlueprint();
            g_editorFont.DrawText2DRight( realX + m_w - 10, realY + 9, 14, seb->m_instance->m_soundName );
        }
    }

    void MouseUp()
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        SoundEventBlueprint *seb = sew->GetSoundEventBlueprint();
        switch( seb->m_instance->m_sourceType )
        {
            case SoundInstance::Sample:
            {
                SoundSelector *selector = new SoundSelector( "SoundName", m_parent->m_name, "sounds/", NULL );
                selector->SetSize( 400, 300 );
                EclRegisterWindow( selector, m_parent );
                break;
            }

            case SoundInstance::SampleGroupRandom:
            {
                SampleGroupEditor *editor = new SampleGroupEditor( "SampleGroupEditor" );
                editor->SetSize( 290, 300 );
                editor->m_seb = seb;
                EclRegisterWindow( editor, m_parent );
                editor->SelectSampleGroup( seb->m_instance->m_soundName );                
                break;
            }
        }

    }
};


class DspEffectButton : public DarwiniaButton
{
public:
    int m_fxIndex;

public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        SoundEventBlueprint *seb = sew->GetSoundEventBlueprint();

        if( seb && seb->m_instance->m_dspFX.ValidIndex( m_fxIndex ) )
        {
            DspHandle *effect = seb->m_instance->m_dspFX[ m_fxIndex ];
            DspBlueprint *blueprint = g_app->m_soundSystem->m_filterBlueprints[effect->m_type];
            SetCaption( blueprint->m_name );
            DarwiniaButton::Render( realX, realY, highlighted, clicked );
        }       
        else if( seb && seb->m_instance->m_dspFX.Size() == m_fxIndex )
        {
            SetCaption( "New DSP Effect" );
            DarwiniaButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        SoundEventBlueprint *seb = sew->GetSoundEventBlueprint();
        
        if( seb && seb->m_instance->m_dspFX.ValidIndex( m_fxIndex ) )
        {
            DspEffectEditor *editor = new DspEffectEditor( "Effect Editor" );
            editor->SetSize( 300, 200 );
            editor->m_effect = seb->m_instance->m_dspFX[m_fxIndex];
            EclRegisterWindow( editor, m_parent );
        }
        else if( seb && seb->m_instance->m_dspFX.Size() == m_fxIndex )
        {
            DspHandle *effect = new DspHandle();
            DspBlueprint *blueprint = g_app->m_soundSystem->m_filterBlueprints[ SoundLibrary3d::DSP_DSOUND_CHORUS ];
            effect->m_type = SoundLibrary3d::DSP_DSOUND_CHORUS;
            for( int i = 0; i < MAX_PARAMS; ++i )
            {
                float defaultVal;
                blueprint->GetParameter( i, &defaultVal );
                effect->m_params[i] = SoundParameter( defaultVal );
                effect->m_params[i].Recalculate();
            }

            seb->m_instance->m_dspFX.PutData( effect );             
        }
    }
};


class DeleteDspEffectButton : public DarwiniaButton
{
public:
    int m_fxIndex;

public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        SoundEventBlueprint *seb = sew->GetSoundEventBlueprint();

        if( seb && seb->m_instance->m_dspFX.ValidIndex( m_fxIndex ) )
        {
            DarwiniaButton::Render( realX, realY, highlighted, clicked );
        }       
    }

    void MouseUp()
    {
        SoundEditorWindow *sew = (SoundEditorWindow *) m_parent;
        SoundEventBlueprint *seb = sew->GetSoundEventBlueprint();
        
        if( seb && seb->m_instance->m_dspFX.ValidIndex( m_fxIndex ) )
        {
            DspHandle *effect = seb->m_instance->m_dspFX[m_fxIndex];
            seb->m_instance->m_dspFX.RemoveData( m_fxIndex );

            delete effect;
        }
    }
};


class PurgeSoundsButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        if( !EclGetWindow( "Purge Sounds" ) )
        {
            PurgeSoundsWindow *purge = new PurgeSoundsWindow( "Purge Sounds" );
            purge->SetSize( 400, 500 );
            EclRegisterWindow( purge, m_parent );
        }
    }
};

void ConvertVolume( float &_volume )
{
    float oldValue = _volume;

    if      ( _volume <= 1.0 )         _volume = 0.0;    
    else if ( _volume <= 1.25 )        _volume = 0.5; 
    else if ( _volume <= 1.5 )         _volume = 1.8;
    else if ( _volume <= 1.75 )        _volume = 3.6;
    else if ( _volume <= 2.0 )         _volume = 5.0;
    else if ( _volume <= 2.25 )        _volume = 6.1;
    else if ( _volume <= 2.5 )         _volume = 7.0;
    else if ( _volume <= 2.75 )        _volume = 7.65;
    else if ( _volume <= 3.0 )         _volume = 8.5;
    else                                _volume = 10.0;

    AppDebugOut( "Converted volume from %2.2 to %2.2\n", oldValue, _volume );
}

class ConvertVolumesButton : public DarwiniaButton
{
public:
    void MouseUp()
    {
        for( int i = 0; i < g_app->m_soundSystem->m_entityBlueprints.Size(); ++i )
        {
            if( g_app->m_soundSystem->m_entityBlueprints.ValidIndex(i) )
            {
                SoundSourceBlueprint *ssb = g_app->m_soundSystem->m_entityBlueprints[i];
                for( int j = 0; j < ssb->m_events.Size(); ++j )
                {
                    SoundEventBlueprint *seb = ssb->m_events[j];
                    SoundParameter *volume = &seb->m_instance->m_volume;
                    switch( volume->m_type )
                    {
                        case SoundParameter::TypeFixedValue:
                            ConvertVolume( volume->m_outputLower );
                            break;

                        case SoundParameter::TypeRangedRandom:
                            ConvertVolume( volume->m_outputLower );
                            ConvertVolume( volume->m_outputUpper );
                            break;

                        case SoundParameter::TypeLinked:
                            ConvertVolume( volume->m_outputLower );
                            ConvertVolume( volume->m_outputUpper );
                            ConvertVolume( volume->m_inputLower );
                            ConvertVolume( volume->m_inputUpper );
                            break;
                    }
                }
            }
        }

        for( int i = 0; i < g_app->m_soundSystem->m_buildingBlueprints.Size(); ++i )
        {
            if( g_app->m_soundSystem->m_buildingBlueprints.ValidIndex(i) )
            {
                SoundSourceBlueprint *ssb = g_app->m_soundSystem->m_buildingBlueprints[i];
                for( int j = 0; j < ssb->m_events.Size(); ++j )
                {
                    SoundEventBlueprint *seb = ssb->m_events[j];
                    SoundParameter *volume = &seb->m_instance->m_volume;
                    switch( volume->m_type )
                    {
                        case SoundParameter::TypeFixedValue:
                            ConvertVolume( volume->m_outputLower );
                            break;

                        case SoundParameter::TypeRangedRandom:
                            ConvertVolume( volume->m_outputLower );
                            ConvertVolume( volume->m_outputUpper );
                            break;

                        case SoundParameter::TypeLinked:
                            ConvertVolume( volume->m_outputLower );
                            ConvertVolume( volume->m_outputUpper );
                            ConvertVolume( volume->m_inputLower );
                            ConvertVolume( volume->m_inputUpper );
                            break;
                    }
                }
            }
        }

        for( int i = 0; i < g_app->m_soundSystem->m_otherBlueprints.Size(); ++i )
        {
            if( g_app->m_soundSystem->m_otherBlueprints.ValidIndex(i) )
            {
                SoundSourceBlueprint *ssb = g_app->m_soundSystem->m_otherBlueprints[i];
                for( int j = 0; j < ssb->m_events.Size(); ++j )
                {
                    SoundEventBlueprint *seb = ssb->m_events[j];
                    SoundParameter *volume = &seb->m_instance->m_volume;
                    switch( volume->m_type )
                    {
                        case SoundParameter::TypeFixedValue:
                            ConvertVolume( volume->m_outputLower );
                            break;

                        case SoundParameter::TypeRangedRandom:
                            ConvertVolume( volume->m_outputLower );
                            ConvertVolume( volume->m_outputUpper );
                            break;

                        case SoundParameter::TypeLinked:
                            ConvertVolume( volume->m_outputLower );
                            ConvertVolume( volume->m_outputUpper );
                            ConvertVolume( volume->m_inputLower );
                            ConvertVolume( volume->m_inputUpper );
                            break;
                    }
                }
            }
        }

    }
};


//*****************************************************************************
// Class SoundEditorWindow
//*****************************************************************************

SoundEditorWindow::SoundEditorWindow( char *_name )
:   DarwiniaWindow( _name ),
    m_eventIndex(-1),
	m_objectSelectorEnabled(false),
    m_eventScrollbar(NULL)
{
    g_app->m_soundSystem->m_propagateBlueprints = true;
}


SoundEditorWindow::~SoundEditorWindow()
{
    g_app->m_soundSystem->m_propagateBlueprints = false;
}


void SoundEditorWindow::SelectEvent( int _eventIndex )
{
    RemoveInstanceEditor();
    m_eventIndex = -1;

    SoundSourceBlueprint *ssb = GetSoundSourceBlueprint();
    if( ssb && ssb->m_events.ValidIndex( _eventIndex ) )
    {
        m_eventIndex = _eventIndex;
        CreateInstanceEditor();
    }    
}


void SoundEditorWindow::StartPlayback( bool looped )
{
    StopPlayback();

    SoundEventBlueprint *seb = GetSoundEventBlueprint();
    if( seb )
    {
        SoundInstance *instance = new SoundInstance();        
        instance->Copy( seb->m_instance );
        instance->m_positionType = SoundInstance::TypeInEditor;
        instance->m_loopType = looped;        
        instance->m_pos = g_app->m_camera->GetPos();
        instance->m_vel = g_zeroVector;
        bool success = g_app->m_soundSystem->InitialiseSound( instance );        
        if( success )
        {
            g_app->m_soundSystem->m_editorInstanceId = instance->m_id;
        }
        else
        {
            delete instance;
        }
    }
}


void SoundEditorWindow::StopPlayback()
{
    SoundInstance *instance = g_app->m_soundSystem->GetSoundInstance( g_app->m_soundSystem->m_editorInstanceId );
    if( instance && instance->IsPlaying() )
    {
        instance->BeginRelease(true);
    }  
}


void SoundEditorWindow::Update()
{
	ObjectGroupMenu *objGroupMenu = (ObjectGroupMenu*)(GetButton("[ObjectGroup]"));
	AppDebugAssert(objGroupMenu);
	ObjectGroupMenu *objTypeMenu = (ObjectGroupMenu*)(GetButton("[ObjectType]"));
	AppDebugAssert(objTypeMenu);

	// Decide whether to enable the feature whereby an object can be selected by
	// clicking on it in the level
	bool selectorEnabled = m_objectSelectorEnabled;
	if (g_app->m_location &&
		_wcsicmp(objGroupMenu->m_caption.m_unicodestring, L"[ObjectGroup]") == 0 &&
		_wcsicmp(objTypeMenu->m_caption.m_unicodestring, L"[ObjectType]") == 0)
	{
		selectorEnabled = true;
	}

	// If we received a mouse click and the object selector was enabled...
	if (selectorEnabled && g_inputManager->controlEvent( ControlEclipseLMouseDown ) )
	{
		Vector3 rayStart, rayDir;
		g_app->m_camera->GetClickRay(g_target->X(), 
									 g_target->Y(), 
									 &rayStart, &rayDir);

		Entity *entity = g_app->m_location->GetEntity(rayStart, rayDir);
		if (entity)
		{
			bool success = objGroupMenu->SelectOption2("Entity");
			AppDebugAssert(success);

			char *typeName = Entity::GetTypeName(entity->m_type);
			success = objTypeMenu->SelectOption2(typeName);
			AppDebugAssert(success);

			m_objectSelectorEnabled = false;
		}

		Building *building = g_app->m_location->GetBuilding(rayStart, rayDir);
		if (building)
		{
			bool success = objGroupMenu->SelectOption2("Building");
			AppDebugAssert(success);

			char *typeName = Building::GetTypeName(building->m_type);
			success = objTypeMenu->SelectOption2(typeName);
			AppDebugAssert(success);

			m_objectSelectorEnabled = false;
		}
	}
}


void SoundEditorWindow::Create()
{
    DarwiniaWindow::Create();
    

    //
    // Sound Source Selectors

    ObjectGroupMenu *objGroup = new ObjectGroupMenu();
    objGroup->SetShortProperties( "[ObjectGroup]", 10, 60, 150, 20, UnicodeString("[ObjectGroup]") );
    RegisterButton( objGroup );
    objGroup->AddOption( "Entity" );
    objGroup->AddOption( "Building" );
    objGroup->AddOption( "Other" );
    objGroup->SelectOption( -1 );
    
    ObjectTypeMenu *objType = new ObjectTypeMenu();
    objType->SetShortProperties( "[ObjectType]", 10, 85, 150, 20, UnicodeString("[ObjectType]") );
    RegisterButton( objType);
    objType->SelectOption( -1 );


    //
    // Control buttons
   
    //PurgeSoundsButton *purge = new PurgeSoundsButton();
    //purge->SetShortProperties( "Purge Sounds", 10, m_h - 75, 150, 20 );
    //RegisterButton(purge);

	SelectObjectButton *sob = new SelectObjectButton();
	sob->SetShortProperties( "Select Object", 10, m_h - 50, 150, 20, UnicodeString("Select Object") );
	RegisterButton(sob);

    SaveAllButton *save = new SaveAllButton();
    save->SetShortProperties( "SAVE", 10, m_h - 25, 150, 20, UnicodeString("Save") );
    RegisterButton( save );


    //
    // Events 

    NewSoundEventMenu *newEvent = new NewSoundEventMenu();
    newEvent->SetShortProperties( "New Event", 10, 160, 150, 20, UnicodeString("NEW EVENT") );
    RegisterButton( newEvent );
    newEvent->SelectOption(-1);
}


void SoundEditorWindow::CreateInstanceEditor()
{
    SoundEventBlueprint *seb = GetSoundEventBlueprint();
    AppDebugAssert( seb );
    AppDebugAssert( seb->m_instance );

    
    //
    // Live parameters

    //SoundPositionButton *spb = new SoundPositionButton();
    //spb->SetShortProperties( "SoundPosition", 190, 50, 150, 100 );
    //RegisterButton( spb );


    //
    // Play controls

    PlayButton *play = new PlayButton();
    play->SetShortProperties( "Play", 400, 50, 80, 20, UnicodeString("Play") );
    RegisterButton( play );

    //PlayRepeatButton *repeat = new PlayRepeatButton();
    //repeat->SetShortProperties( "Loop", 400, 75, 80, 20 );
    //RegisterButton( repeat );

    StopButton *stop = new StopButton();
    stop->SetShortProperties( "Stop", 400, 75, 80, 20, UnicodeString("Stop") );
    RegisterButton( stop );


    int xPos = 190;
    float inset = 95;
    int yPos = 150;
    int yDif = 25;
    int w = 280;
    int h = 20;


    //
    // Inverted box

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", xPos-10, yPos-10, w+25, 340, UnicodeString("invert") );
    RegisterButton( box );

    //
    // Position Type

    DropDownMenu *soundType = new DropDownMenu(false);
    soundType->SetShortProperties( "PositionType", xPos + inset, yPos, w - inset, h, UnicodeString("PositionType") );
    RegisterButton( soundType );
    for( int i = 0; i < SoundInstance::NumPositionTypes; ++i )
    {
        soundType->AddOption( SoundInstance::GetPositionTypeName(i) );
    }
    soundType->RegisterInt( &seb->m_instance->m_positionType );    


    //
    // Loop type

    DropDownMenu *loopType = new DropDownMenu(false);
    loopType->SetShortProperties( "LoopType", xPos + inset, yPos += yDif, w - inset, h, UnicodeString("LoopType") );
    RegisterButton( loopType );
    for( int i = 0; i < SoundInstance::NumLoopTypes; ++i )
    {
        loopType->AddOption( SoundInstance::GetLoopTypeName(i) );    
    }
    loopType->RegisterInt( &seb->m_instance->m_loopType );


    //
    // Instance type

    DropDownMenu *instanceType = new DropDownMenu(false);
    instanceType->SetShortProperties( "InstanceType", xPos + inset, yPos += yDif, w - inset, h, UnicodeString("InstanceType") );
    RegisterButton( instanceType );
    for( int i = 0; i < SoundInstance::NumInstanceTypes; ++i )
    {
        instanceType->AddOption( SoundInstance::GetInstanceTypeName(i) );
    }
    instanceType->RegisterInt( &seb->m_instance->m_instanceType );
    

    //
    // Team Match Type

    DropDownMenu *teamMatchType = new DropDownMenu(false);
    teamMatchType->SetShortProperties( "TeamMatchType", xPos + inset, yPos += yDif, w - inset, h, UnicodeString("TeamMatchType") );
    RegisterButton( teamMatchType );
    for( int i = 0; i < SoundInstance::NumTeamMatchTypes; ++i )
    {
        teamMatchType->AddOption( SoundInstance::GetTeamMatchTypeName(i) );
    }
    teamMatchType->RegisterInt( &seb->m_instance->m_teamMatchType );


    //
    // Source type

    DropDownMenu *sourceType = new DropDownMenu(false);
    sourceType->SetShortProperties( "SourceType", xPos + inset, yPos += yDif, w - inset, h, UnicodeString("SourceType") );
    RegisterButton( sourceType );
    for( int i = 0; i < SoundInstance::NumSourceTypes; ++i )
    {
        sourceType->AddOption( SoundInstance::GetSourceTypeName(i) );
    }
    sourceType->RegisterInt( &seb->m_instance->m_sourceType );


    //
    // Sound name

    SoundSelectorButton *selector = new SoundSelectorButton();
    selector->SetShortProperties( "SoundName", xPos, yPos += yDif, w, h, UnicodeString("SoundName") );
    RegisterButton( selector );


    //
    // Static parameters

    CreateValueControl          ( "MinDistance",  &seb->m_instance->m_minDistance, yPos += yDif, 0.5, 1.0, 10000.0, NULL, xPos, w );

    CreateSoundParameterButton  ( this, "Volume",       &seb->m_instance->m_volume, yPos += yDif, 0.0, 10.0, NULL, xPos, w, h );
    CreateSoundParameterButton  ( this, "Frequency",    &seb->m_instance->m_freq, yPos += yDif, 0.0, 3.0, NULL, xPos, w, h );
    CreateSoundParameterButton  ( this, "Attack",       &seb->m_instance->m_attack, yPos += yDif, 0.0, 60.0, NULL, xPos, w, h );
    CreateSoundParameterButton  ( this, "Sustain",      &seb->m_instance->m_sustain, yPos += yDif, 0.0, 300.0, NULL, xPos, w, h );
    CreateSoundParameterButton  ( this, "Release",      &seb->m_instance->m_release, yPos += yDif, 0.0, 60.0, NULL, xPos, w, h );
    CreateSoundParameterButton  ( this, "LoopDelay",    &seb->m_instance->m_loopDelay, yPos += yDif, 0.0, 60.0, NULL, xPos, w, h );

    //
    // Effects

    yPos += 50;
    
    for( int i = 0; i < 3; ++i )
    {
        char name[64];
        sprintf( name, "Effect %d", i );
        DspEffectButton *button = new DspEffectButton();
        button->SetShortProperties( name, xPos, yPos + i * 19, w-70, 17, UnicodeString(name) );
        button->m_fxIndex = i;
        RegisterButton( button );

        sprintf( name, "Delete %d", i );
        DeleteDspEffectButton *deleteButton = new DeleteDspEffectButton();
        deleteButton->SetShortProperties( name, xPos + button->m_w + 10, yPos + i * 19, 60, 17, UnicodeString("Delete") );
        deleteButton->m_fxIndex = i;
        RegisterButton( deleteButton );
    }


    //
    // Control buttons

    DeleteEventButton *deleteEvent = new DeleteEventButton();
    deleteEvent->SetShortProperties( "Delete Event", m_w - 100, m_h - 30, 90, 18, UnicodeString("Delete Event") );
    RegisterButton( deleteEvent );
}


void SoundEditorWindow::RemoveInstanceEditor()
{
    RemoveButton( "SoundPosition" );
    
    RemoveButton( "Play" );
    RemoveButton( "Loop" );
    RemoveButton( "Stop" );

    RemoveButton( "invert" );
    
    RemoveButton( "PositionType" );
    RemoveButton( "InstanceType" );
    RemoveButton( "LoopType" );
    RemoveButton( "SourceType" );
    RemoveButton( "TeamMatchType" );

    RemoveValueControl( "SoundName" );
    RemoveValueControl( "MinDistance" );

    RemoveSoundParameterButton( this, "Volume" );
    RemoveSoundParameterButton( this, "Frequency" );
    RemoveSoundParameterButton( this, "Attack" );
    RemoveSoundParameterButton( this, "Sustain" );
    RemoveSoundParameterButton( this, "Release" );
    RemoveSoundParameterButton( this, "LoopDelay" );

    for( int i = 0; i < 5; ++i )
    {
        char name[64];
        sprintf( name, "Effect %d", i );
        RemoveButton( name );

        sprintf( name, "Delete %d", i );
        RemoveButton( name );
    }

    RemoveButton( "Delete Event" );
}


void SoundEditorWindow::Render( bool hasFocus )
{
    DarwiniaWindow::Render( hasFocus );
    
    g_editorFont.DrawText2D( m_x + 10, m_y + 35, 20, "SOUND SOURCE" );
    g_editorFont.DrawText2D( m_x + 10, m_y + 140, 20, "EVENTS" );
    g_editorFont.DrawText2D( m_x + 200, m_y + 35, 20, "PARAMETERS" );
    //g_editorFont.DrawText2D( m_x + 200, m_y + 455, 20, "EFFECTS" );

    float y = m_y + 160;
    float h = 25;

    g_editorFont.DrawText2D( m_x + 190, y, 11, "Position" );
    g_editorFont.DrawText2D( m_x + 190, y += h, 11, "LoopType" );
    g_editorFont.DrawText2D( m_x + 190, y += h, 11, "InstanceType" );
    g_editorFont.DrawText2D( m_x + 190, y += h, 11, "TeamMatchType" );
    g_editorFont.DrawText2D( m_x + 190, y += h, 11, "SourceType" );
    
	ObjectGroupMenu *objGroupMenu = (ObjectGroupMenu*)(GetButton("[ObjectGroup]"));
	AppDebugAssert(objGroupMenu);
	ObjectGroupMenu *objTypeMenu = (ObjectGroupMenu*)(GetButton("[ObjectType]"));
	AppDebugAssert(objTypeMenu);

	bool selectorEnabled = m_objectSelectorEnabled;
	if (g_app->m_location &&
		_wcsicmp(objGroupMenu->m_caption.m_unicodestring, L"[ObjectGroup]") == 0 &&
		_wcsicmp(objTypeMenu->m_caption.m_unicodestring, L"[ObjectType]") == 0)
	{
		selectorEnabled = true;
	}

	if (selectorEnabled)
	{
		EclButton *sob = GetButton("Select Object");
		AppDebugAssert(sob);
		int time = (float)g_gameTime * 2.0;
		if (time & 1)
		{
			sob->Render( m_x + sob->m_x, m_y + sob->m_y, true, false );
		}
	}

    glColor4f( 1.0, 1.0, 1.0, 0.5 );
    glBegin( GL_LINES );
        glVertex2i( m_x + 170, m_y + 30 );
        glVertex2i( m_x + 170, m_y + m_h - 20 );
    glEnd();


    //
    // Render current editor sound

    SoundInstance *instance = g_app->m_soundSystem->GetSoundInstance( g_app->m_soundSystem->m_editorInstanceId );

    y = m_y + 70;
    glColor4f( 1.0, 1.0, 1.0, 1.0 );

    if( instance )
    {
        g_editorFont.DrawText2D( m_x + 190, y, 11, "PLAYING" );       
        y+=5;
        g_editorFont.DrawText2D( m_x + 190, y+=15, 11, "Sample  : %s", instance->m_soundName );
        g_editorFont.DrawText2D( m_x + 190, y+=15, 11, "Volume  : %2.1", instance->m_channelVolume );

        if( instance->m_loopType == SoundInstance::LoopedADSR )
        {
            g_editorFont.DrawText2D( m_x + 190, y+=15, 11, "ADSR    : %s", SoundInstance::GetADSRTypeName( instance->m_adsrState ) );
        }

        g_editorFont.DrawText2D( m_x + 190, y+=15, 11, "Freq    : %2.1", instance->m_freq.GetOutput() );
    }
    else
    {
        g_editorFont.DrawText2D( m_x + 190, y, 11, "STOPPED" );        
    }



    //
    // Render all running sounds

    g_app->m_renderer->SetupMatricesFor3D();

    glEnable        ( GL_TEXTURE_2D );
    glBindTexture   ( GL_TEXTURE_2D, g_app->m_resource->GetTexture( "sprites/sound.bmp" ) );
	glTexParameteri	( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri	( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
    glDepthMask     ( false );
    
    for( int i = 0; i < g_app->m_soundSystem->m_numChannels; ++i )
    {
        SoundInstanceId soundId = g_app->m_soundSystem->m_channels[i];
        SoundInstance *instance = g_app->m_soundSystem->GetSoundInstance( soundId );
        if( instance && 
            instance->m_positionType != SoundInstance::Type2D &&
            instance->m_positionType != SoundInstance::TypeInEditor )
        {
            float distance = ( instance->m_pos - g_app->m_camera->GetPos() ).Mag();
            float size = 0.5 * iv_sqrt(distance) * iv_sqrt(instance->m_channelVolume);
            Vector3 up = g_app->m_camera->GetUp() * size * 0.5;
            Vector3 right = g_app->m_camera->GetRight() * size * -0.5;

            int priority = instance->m_calculatedPriority;
            float colour = 0.5 + 0.5 * (float) priority / 10.0;
            if( colour > 0.9 ) colour = 0.9;
            glColor4f( 1.0, 1.0, 1.0, colour );

            Vector3 pos = instance->m_pos;

            glBegin( GL_QUADS );
                glTexCoord2i( 0, 0 );       glVertex3dv( (pos - up - right).GetData() );
                glTexCoord2i( 1, 0 );       glVertex3dv( (pos - up + right).GetData() );
                glTexCoord2i( 1, 1 );       glVertex3dv( (pos + up + right).GetData() );
                glTexCoord2i( 0, 1 );       glVertex3dv( (pos + up - right).GetData() );
            glEnd();
        }
    }    
        
    glDepthMask     ( true );
    glDisable       ( GL_TEXTURE_2D );
    
    g_app->m_renderer->SetupMatricesFor2D();
}


SoundSourceBlueprint *SoundEditorWindow::GetSoundSourceBlueprint()
{
    DropDownMenu *objGroupMenu = (DropDownMenu *) GetButton( "[ObjectGroup]" );
    ObjectTypeMenu *objTypeMenu = (ObjectTypeMenu *) GetButton( "[ObjectType]" );

    AppDebugAssert( objGroupMenu );
    AppDebugAssert( objTypeMenu );

	UnicodeString objGroup;
	UnicodeString objType;

	objGroupMenu->GetSelectionName(objGroup);
	objTypeMenu->GetSelectionName(objType);

	if (objGroup.m_unicodestring && objType.m_unicodestring)
	{
		if (wcsicmp(L"Entity", objGroup.m_unicodestring) == 0)
		{
			const char* objTypeChar = objType.GetCharArray();
			int i = Entity::GetTypeId(objTypeChar);
			if (i == -1)
			{
				return NULL;
			}
			return g_app->m_soundSystem->m_entityBlueprints[i];
		}
		else if (wcsicmp(L"Building", objGroup.m_unicodestring) == 0)
		{
			const char* objTypeChar = objType.GetCharArray();
			int i = Building::GetTypeId(objTypeChar);
			if (i == -1)
			{
				return NULL;
			}
			return g_app->m_soundSystem->m_buildingBlueprints[i];
		}
		else
		{
			const char* objTypeChar = objType.GetCharArray();
			int i = SoundSourceBlueprint::GetSoundSoundType(objTypeChar);
			if (i == -1)
			{
				return NULL;
			}
			return g_app->m_soundSystem->m_otherBlueprints[i];
		}
	}

    return NULL;
}


SoundEventBlueprint *SoundEditorWindow::GetSoundEventBlueprint()
{
    SoundSourceBlueprint *ssb = GetSoundSourceBlueprint();
    if( ssb && ssb->m_events.ValidIndex( m_eventIndex ) )
    {
        return ssb->m_events[m_eventIndex];
    }

    return NULL;
}


void SoundEditorWindow::GetSelectionName(char *_buf)
{
//    DropDownMenu *objGroupMenu = (DropDownMenu *) GetButton( "[ObjectGroup]" );
    DropDownMenu *objTypeMenu = (DropDownMenu *) GetButton( "[ObjectType]" );
	UnicodeString temp;
	objTypeMenu->GetSelectionName(temp);
	strcpy(_buf, (char*)&(temp));
//
//    AppDebugAssert( objGroupMenu );
//    AppDebugAssert( objTypeMenu );
//
//    int objGroup = objGroupMenu->GetSelectionIndex();
//    int objType = objTypeMenu->GetSelectionIndex();
//
//    switch( objGroup )
//    {
//        case 0 :    
//            strcpy(_buf, Entity::GetTypeName(objType));
//            break;
//
//        case 1:
//            strcpy(_buf, Building::GetTypeName(objType));
//            break;    
//            
//        case 2:
//			strcpy(_buf, SoundSourceBlueprint::GetSoundSourceName(objType));
//            break;
//    }
}


void SoundEditorWindow::CreateSoundParameterButton( DarwiniaWindow *_window,
                                                    char *name, SoundParameter *parameter, int y, 
							                        float _minOutput, float _maxOutput,
                                                    DarwiniaButton *callback, int x, int w, int h )
{
    if( x == -1 ) x = 10;
    if( w == -1 ) w = _window->m_w - x*2;

    SoundParameterButton *button = new SoundParameterButton();
    button->SetShortProperties( name, x, y, w, h, UnicodeString(name) );
    button->m_parameter = parameter;
    button->m_minOutput = _minOutput;
    button->m_maxOutput = _maxOutput;
    _window->RegisterButton( button );
}


void SoundEditorWindow::RemoveSoundParameterButton( DarwiniaWindow *_window, char *name )
{
    _window->RemoveButton( name );
}


//*****************************************************************************
// Class DspEffectTypeMenu
//*****************************************************************************

class DspEffectTypeMenu : public DropDownMenu
{
    void SelectOption( int _value )
    {
        int index = 0;
        char name[64];
        sprintf( name, "param %d", index );
        while( m_parent->GetButton(name) )
        {
            m_parent->RemoveButton(name);
            ++index;
            sprintf( name, "param %d", index);
        }
        
        bool changed = m_currentOption != -1 &&
                       m_currentOption != _value;

        DropDownMenu::SelectOption( _value );
		int optionIndex = FindValue(_value);
        AppDebugAssert( g_app->m_soundSystem->m_filterBlueprints[ optionIndex ] );
        DspBlueprint *seb = g_app->m_soundSystem->m_filterBlueprints[ optionIndex ];
        DspEffectEditor *see = (DspEffectEditor *) m_parent;
        
        for( int i = 0; i < seb->m_params.Size(); ++i )
        {            
            char name[64];
            sprintf( name, "param %d", i );

            SoundParameter *param = &see->m_effect->m_params[i];

            char *paramName;
            float minOutput, maxOutput, defaultOutput;
            paramName = seb->GetParameter( i, &minOutput, &maxOutput, &defaultOutput );

            if( changed )
            {
                param->m_type = SoundParameter::TypeFixedValue;
                param->m_outputLower = defaultOutput;
            }

            param->Recalculate();

            SoundEditorWindow::CreateSoundParameterButton( (DarwiniaWindow *) m_parent, name, 
                                                            param, 60 + i * 20, 
                                                            minOutput, maxOutput );
            m_parent->GetButton( name )->SetCaption( paramName );
        }        
    }
};


//*****************************************************************************
// Class DspEffectEditor
//*****************************************************************************

DspEffectEditor::DspEffectEditor( char *_name )
:   DarwiniaWindow(_name),
    m_effect(NULL)
{
}

void DspEffectEditor::Create()
{
    DarwiniaWindow::Create();

    int y = 30;
    int h = 20;

    DspEffectTypeMenu *fxType = new DspEffectTypeMenu();
    fxType->SetShortProperties( "Effect Type", 10, y, m_w - 20, -1, UnicodeString("Effect Type") );
    RegisterButton( fxType );
    for( int i = 0; i < g_app->m_soundSystem->m_filterBlueprints.Size(); ++i )
    {
        DspBlueprint *effect = g_app->m_soundSystem->m_filterBlueprints[i];
        fxType->AddOption( effect->m_name );
    }
    
    fxType->RegisterInt( &m_effect->m_type );
}



//*****************************************************************************
// Class SampleGroupEditor
//*****************************************************************************

class SampleGroupFileDialog : public FileDialog
{
public:
    SampleGroup *m_group;
    int m_index;
	
    SampleGroupFileDialog( char const *name, char const *parent, char const *path, char const *filter)
    :   FileDialog( name, parent, path, filter, true )
    {
    }

    void FileSelected( char *_filename )
    {
        char const *extensionRemoved = RemoveExtension( _filename );
        char const *err = g_app->m_soundSystem->IsSoundSourceOK(extensionRemoved);
		if (err)
		{
			//MessageDialog *win = new MessageDialog("Alert", err);
			//EclRegisterWindow(win);
			return;
		}

        if( m_group->m_samples.ValidIndex( m_index ) )
        {
            m_group->m_samples.RemoveData(m_index);
            
            char *filenameCopy = strdup( extensionRemoved );
            m_group->m_samples.PutDataAtIndex( filenameCopy, m_index );
        }
        else
        {
            char *filenameCopy = strdup( extensionRemoved );
            m_group->m_samples.PutData( filenameCopy );
        }
    }
};


class SampleGroupSampleName : public DarwiniaButton
{
public:
    int m_index;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        EclButton *button = m_parent->GetButton( "SampleGroup" );
        const char *groupName = button->m_caption;
        SampleGroup *group = g_app->m_soundSystem->GetSampleGroup( groupName );

        if( group && group->m_samples.ValidIndex(m_index) )
        {
            SetCaption( group->m_samples[m_index] );
            DarwiniaButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        EclButton *button = m_parent->GetButton( "SampleGroup" );
        const char *groupName = button->m_caption;
        SampleGroup *group = g_app->m_soundSystem->GetSampleGroup( groupName );
    }
};


class SampleGroupSampleDelete : public DarwiniaButton
{
public:
    int m_index;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        EclButton *button = m_parent->GetButton( "SampleGroup" );
        const char *groupName = button->m_caption;
        SampleGroup *group = g_app->m_soundSystem->GetSampleGroup( groupName );

        int baseOffset = ((SampleGroupEditor *) m_parent)->m_scrollbar->m_currentValue;
        int index = m_index + baseOffset;

        if( group && group->m_samples.ValidIndex(index) )
        {
            DarwiniaButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        EclButton *button = m_parent->GetButton( "SampleGroup" );
        const char *groupName = button->m_caption;
        SampleGroup *group = g_app->m_soundSystem->GetSampleGroup( groupName );

        int baseOffset = ((SampleGroupEditor *) m_parent)->m_scrollbar->m_currentValue;
        int index = m_index + baseOffset;

        if( group && group->m_samples.ValidIndex(index) )
        {
            group->m_samples.RemoveData(index);
        }
    }
};


class SampleGroupAddSample : public DarwiniaButton
{
    void MouseUp()
    {
        EclButton *button = m_parent->GetButton( "SampleGroup" );
        const char *groupName = button->m_caption;
        SampleGroup *group = g_app->m_soundSystem->GetSampleGroup( groupName );
        
        if( group )
        {
            SampleGroupFileDialog *file = new SampleGroupFileDialog("SampleGroupSelectFile", m_parent->m_name, "sounds/", NULL );
            file->SetSize( 400, 300 );
            file->m_group = group;
            file->m_index = -1;
            EclRegisterWindow( file, m_parent );
        }
    }
};


class SampleGroupSelector : public DropDownMenu
{
    void SelectOption( int _option )
    {
        DropDownMenu::SelectOption( _option );

        SampleGroupEditor *sge = (SampleGroupEditor *) m_parent;
        sge->m_seb->m_instance->SetSoundName( m_options[m_currentOption]->m_word );
    }
};


class NewSampleGroupButton : public DarwiniaButton
{
    void MouseUp()
    {
        SampleGroup *newGroup = g_app->m_soundSystem->NewSampleGroup();
        SampleGroupEditor *sge = (SampleGroupEditor *) m_parent;
        sge->Remove();
        sge->Create();
        sge->SelectSampleGroup( newGroup->m_name );
    }
};


class RenameSampleGroupCommit : public DarwiniaButton
{
    void MouseUp()
    {
        RenameSampleGroupWindow *window = (RenameSampleGroupWindow *) m_parent;
        bool success = g_app->m_soundSystem->RenameSampleGroup( window->m_oldName, window->m_newName );
        if( success )
        {
            EclRemoveWindow( m_parent->m_name );        
        }
        else
        {
			MessageDialog *win = new MessageDialog(
					"Failed to Rename Sample Group", 
					"A SampleGroup already exists with that name");
			EclRegisterWindow(win);
        }        
    }
};


RenameSampleGroupWindow::RenameSampleGroupWindow( char *_name )
:   DarwiniaWindow( _name )
{
}

void RenameSampleGroupWindow::Create()
{
    DarwiniaWindow::Create();

    CreateValueControl( "Name", m_newName, 20, 0.0, 0.0, sizeof(m_newName) );

    RenameSampleGroupCommit *commit = new RenameSampleGroupCommit();
    commit->SetShortProperties( "OK", 10, 40, 110, -1, UnicodeString("OK") );
    RegisterButton( commit );

    CloseButton *closeButton = new CloseButton();
    closeButton->SetShortProperties( "Cancel", 130, 40, 110, -1, UnicodeString("Cancel") );
    RegisterButton( closeButton );
}


class RenameSampleGroupButton : public DarwiniaButton
{
    void MouseUp()
    {
        EclButton *button = m_parent->GetButton( "SampleGroup" );
        const char *groupName = button->m_caption;

        RenameSampleGroupWindow *window = new RenameSampleGroupWindow( "Rename Sample Group" );
        window->SetSize( 250, 75 );
        strcpy( window->m_oldName, groupName );
        strcpy( window->m_newName, groupName );
        EclRegisterWindow( window, m_parent );

        EclRemoveWindow( m_parent->m_name );
    }
};


SampleGroupEditor::SampleGroupEditor( char *_name )
:   DarwiniaWindow( _name ),
    m_seb(NULL),
    m_scrollbar(NULL)
{
    m_scrollbar = new ScrollBar(this);
}


void SampleGroupEditor::SelectSampleGroup( char *_group )
{
    DropDownMenu *sampleGroup = (DropDownMenu *) GetButton( "SampleGroup" );
    sampleGroup->SelectOption2( _group );
}


void SampleGroupEditor::Create()
{
    DarwiniaWindow::Create();
        
    //
    // Sample group selector / new Sample Group

    SampleGroupSelector *sampleGroup = new SampleGroupSelector();
    sampleGroup->SetShortProperties( "SampleGroup", 10, 20, m_w - 100, 18, UnicodeString("SampleGroup") );
    RegisterButton( sampleGroup );
    for( int i = 0; i < g_app->m_soundSystem->m_sampleGroups.Size(); ++i )
    {
        if( g_app->m_soundSystem->m_sampleGroups.ValidIndex(i) )
        {
            SampleGroup *group = g_app->m_soundSystem->m_sampleGroups[i];
            sampleGroup->AddOption( group->m_name );
        }
    }
    
    NewSampleGroupButton *newGroup = new NewSampleGroupButton();
    newGroup->SetShortProperties( "New Group", sampleGroup->m_x + sampleGroup->m_w + 10, 20, 70, 18, UnicodeString("New Group") );
    RegisterButton( newGroup );

    //
    // Rename group

    RenameSampleGroupButton *renameGroup = new RenameSampleGroupButton();
    renameGroup->SetShortProperties( "Rename Group", 10, 50, m_w - 20, 17, UnicodeString("Rename Group") );
    RegisterButton( renameGroup );


    //
    // Add Samples to group

    SampleGroupAddSample *addSample = new SampleGroupAddSample();
    addSample->SetShortProperties( "Add Samples", 10, 70, m_w - 20, 17, UnicodeString("Add Samples") );
    RegisterButton( addSample );

    
    //
    // Edit / Remove Samples from group

    int numToCreate = (m_h - 100) / 20;

    for( int i = 0; i < numToCreate; ++i )
    {
        char name[64];
        sprintf( name, "SampleGroupSampleDelete %d", i );
        SampleGroupSampleDelete *sd = new SampleGroupSampleDelete();
        sd->SetShortProperties( name, 10, 100 + i * 20, 70, 17, "Delete" );
        sd->m_index = i;
        RegisterButton( sd );
    }


    //
    // Scrollbar

    m_scrollbar->Create( "Scrollbar", m_w - 20, 100, 15, m_h - 110, 0, numToCreate );
}


void SampleGroupEditor::Remove()
{
    DarwiniaWindow::Remove();
    
    RemoveButton( "SampleGroup" );
    RemoveButton( "New Group" );
    RemoveButton( "Add Samples" );
    
    int i = 0;
    while( true )
    {
        char name[64];
        sprintf( name, "SampleGroupSampleName %d", i );
        if( !GetButton(name) ) break;
        RemoveButton( name );

        sprintf( name, "SampleGroupSampleDelete %d", i );
        RemoveButton( name );

        ++i;
    }

    m_scrollbar->Remove();
}


void SampleGroupEditor::Render( bool _hasFocus )
{
    DarwiniaWindow::Render( _hasFocus );

    EclButton *button = GetButton( "SampleGroup" );
    const char *groupName = button->m_caption;
    SampleGroup *group = g_app->m_soundSystem->GetSampleGroup( groupName );

    
    //
    // Update the scrollbar
    // Draw the samples in the group
    
    if( group )
    {
        m_scrollbar->SetNumRows( group->m_samples.Size() );

        int numToCreate = (m_h - 100) / 20;
        int baseOffset = m_scrollbar->m_currentValue;
        for( int i = 0; i < numToCreate; ++i )
        {
            int index = baseOffset + i;
            if( group->m_samples.ValidIndex(index) )
            {
                char *thisSample = group->m_samples[index];
                g_editorFont.DrawText2D( m_x + 90, m_y + 107 + i * 20, 14, thisSample );
            }
        }
    }
    else
    {
        m_scrollbar->SetNumRows( 0 );
    }    
}


//*****************************************************************************
// Class PurgeSoundsWindow
//*****************************************************************************

class DeleteSoundButton : public DarwiniaButton
{
public:
    int m_index;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        PurgeSoundsWindow *parent = (PurgeSoundsWindow *) m_parent;
        int baseOffset = parent->m_scrollBar->m_currentValue;
        int index = m_index + baseOffset;

        if( parent->m_fileList.ValidIndex( index ) )
        {
            DarwiniaButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        PurgeSoundsWindow *parent = (PurgeSoundsWindow *) m_parent;
        int baseOffset = parent->m_scrollBar->m_currentValue;
        int index = m_index + baseOffset;

        if( parent->m_fileList.ValidIndex( index ) )
        {
			if( _wcsicmp( m_caption.m_unicodestring, L"delete" ) == 0 )
            {
                SetCaption( UnicodeString("!DELETE!") );
            }
            else
            {
                char *filename = parent->m_fileList[ index ];
                char fullFilename[256];
                sprintf( fullFilename, "data/sounds/%s", filename );
                DeleteThisFile( fullFilename );
                parent->RefreshFileList();
                SetCaption( UnicodeString("delete") );
            }
        }
    }
};


class RefreshFileListButton : public DarwiniaButton
{
    void MouseUp()
    {
        PurgeSoundsWindow *parent = (PurgeSoundsWindow *) m_parent;
        parent->RefreshFileList();
    }
};


class DeleteAllButton : public DarwiniaButton 
{
    void MouseUp()
    {
		if( _wcsicmp( m_caption.m_unicodestring, L"Delete All" ) == 0 )
        {
            SetCaption( UnicodeString("!DELETE ALL!") );
        }
        else
        {
            PurgeSoundsWindow *parent = (PurgeSoundsWindow *) m_parent;
            for( int i = 0; i < parent->m_fileList.Size(); ++i )
            {
                char *filename = parent->m_fileList[i];
                char fullFilename[256];
                sprintf( fullFilename, "data/sounds/%s", filename );
                DeleteThisFile( fullFilename );
            }
            parent->RefreshFileList();
            SetCaption( UnicodeString("Delete All") );
        }
    }
};


PurgeSoundsWindow::PurgeSoundsWindow( char *_name )
:   DarwiniaWindow( _name ),
    m_scrollBar(NULL)
{
    m_scrollBar = new ScrollBar( this );
}


void PurgeSoundsWindow::Create()
{
    DarwiniaWindow::Create();

    int numSlots = (m_h - 75) / 16;

    for( int i = 0; i < numSlots; ++i )
    {
        char name[64];
        sprintf( name, "DeleteSound %d", i );
        DeleteSoundButton *deleteSound = new DeleteSoundButton();
        deleteSound->SetShortProperties( name, 10, 42 + i * 16, 60, 15, "delete" );
        deleteSound->m_index = i;
        RegisterButton( deleteSound );
    }

    RefreshFileListButton *refresh = new RefreshFileListButton();
    refresh->SetShortProperties( "Refresh", 10, m_h - 30, (m_w - 40)/2, -1, UnicodeString("Refresh") );
    RegisterButton( refresh );

    DeleteAllButton *deleteAll = new DeleteAllButton();
    deleteAll->SetShortProperties( "Delete All", refresh->m_x + refresh->m_w + 10, m_h - 30, (m_w - 40)/2, -1, UnicodeString("Delete All") );
    RegisterButton( deleteAll );
    
    m_scrollBar->Create( "Scrollbar", m_w - 20, 42, 15, m_h - 75, 0, numSlots );

    RefreshFileList();
}


void PurgeSoundsWindow::Remove()
{
    DarwiniaWindow::Remove();
    
    m_fileList.EmptyAndDelete();

    int i = 0;
    char name[64];
    sprintf( name, "DeleteSound %d", i );
    while( GetButton( name ) )
    {
        RemoveButton( name );

        ++i;
        sprintf( name, "DeleteSound %d", i );
    }

    RemoveButton( "Refresh" );
    RemoveButton( "Delete All" );

    m_scrollBar->Remove();
}


void PurgeSoundsWindow::Render( bool _hasFocus )
{
    DarwiniaWindow::Render( _hasFocus );
    
    g_editorFont.DrawText2DCentre( m_x + m_w/2, m_y + 25, 16, "Unused Sounds : " );

    int baseOffset = m_scrollBar->m_currentValue;
    int numSlots = m_scrollBar->m_winSize;

    for( int i = 0; i < numSlots; ++i )
    {
        int index = i + baseOffset;        
        if( m_fileList.ValidIndex(index) )
        {
            char *filename = m_fileList[index];
            g_editorFont.DrawText2D( m_x + 80, m_y + 50 + i * 16, 13, filename );
        }
    }
}


void PurgeSoundsWindow::RefreshFileList()
{
    m_fileList.EmptyAndDelete();

    LList<char *> *allFiles = ListDirectory( "data/sounds/", "*.*", false );
    
    for( int i = 0; i < allFiles->Size(); ++i )
    {
        char *filename = allFiles->GetData(i);
        const char *strippedFilename = RemoveExtension( filename );
        if( !g_app->m_soundSystem->IsSampleUsed( strippedFilename ) )
        {
            m_fileList.PutData(filename);
        }
        else
        {
            delete filename;
        }
    }

    delete allFiles;    

    m_scrollBar->SetNumRows( m_fileList.Size() );
}



#endif // SOUND_EDITOR
