//========= Copyright Valve Corporation ============//

#include "openvr_gl.h"
//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CMainApplication::CMainApplication( int argc, char *argv[] )
	: m_pCompanionWindow(NULL)
	, m_pContext(NULL)
	, m_nCompanionWindowWidth( 1280 )
	, m_nCompanionWindowHeight( 720 )
	, m_unSceneProgramID( 0 )
	, m_unCompanionWindowProgramID( 0 )
	, m_unControllerTransformProgramID( 0 )
	, m_unRenderModelProgramID( 0 )
	, m_pHMD( NULL )
	, m_bDebugOpenGL( false )
	, m_bVerbose( false )
	, m_bPerf( false )
	, m_bVblank( false )
	, m_bGlFinishHack( true )
	, m_glControllerVertBuffer( 0 )
	, m_unControllerVAO( 0 )
	, m_glPointCloudVertBuffer( 0 )
	, m_unPointCloudVAO( 0 )
	, m_glColorTrisVertBuffer( 0 )
	, m_unColorTrisVAO( 0 )
	, m_unSceneVAO( 0 )
	, m_nSceneMatrixLocation( -1 )
	, m_nControllerMatrixLocation( -1 )
	, m_nRenderModelMatrixLocation( -1 )
	, m_iTrackedControllerCount( 0 )
	, m_iTrackedControllerCount_Last( -1 )
	, m_iValidPoseCount( 0 )
	, m_iValidPoseCount_Last( -1 )
	, m_iSceneVolumeInit( 20 )
	, m_strPoseClasses("")
	, m_bShowCubes( true )
	, m_bShowControllers( true )
{

	for( int i = 1; i < argc; i++ )
	{
		if( !stricmp( argv[i], "-gldebug" ) )
		{
			m_bDebugOpenGL = true;
		}
		else if( !stricmp( argv[i], "-verbose" ) )
		{
			m_bVerbose = true;
		}
		else if( !stricmp( argv[i], "-novblank" ) )
		{
			m_bVblank = false;
		}
		else if( !stricmp( argv[i], "-noglfinishhack" ) )
		{
			m_bGlFinishHack = false;
		}
		else if( !stricmp( argv[i], "-noprintf" ) )
		{
			g_bPrintf = false;
		}
		else if ( !stricmp( argv[i], "-cubevolume" ) && ( argc > i + 1 ) && ( *argv[ i + 1 ] != '-' ) )
		{
			m_iSceneVolumeInit = atoi( argv[ i + 1 ] );
			i++;
		}
	}
	// other initialization tasks are done in BInit
	memset(m_rDevClassChar, 0, sizeof(m_rDevClassChar));
};


//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CMainApplication::~CMainApplication()
{
	// work is done in Shutdown
	dprintf( "Shutdown" );
}


//-----------------------------------------------------------------------------
// Purpose: Helper to get a string from a tracked device property and turn it
//			into a std::string
//-----------------------------------------------------------------------------
std::string GetTrackedDeviceString( vr::TrackedDeviceIndex_t unDevice, vr::TrackedDeviceProperty prop, vr::TrackedPropertyError *peError = NULL )
{
	uint32_t unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty( unDevice, prop, NULL, 0, peError );
	if( unRequiredBufferLen == 0 )
		return "";

	char *pchBuffer = new char[ unRequiredBufferLen ];
	unRequiredBufferLen = vr::VRSystem()->GetStringTrackedDeviceProperty( unDevice, prop, pchBuffer, unRequiredBufferLen, peError );
	std::string sResult = pchBuffer;
	delete [] pchBuffer;
	return sResult;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMainApplication::BInit()
{
	if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
	{
		printf("%s - SDL could not initialize! SDL Error: %s\n", __FUNCTION__, SDL_GetError());
		return false;
	}

	// Loading the SteamVR Runtime
	vr::EVRInitError eError = vr::VRInitError_None;
	m_pHMD = vr::VR_Init( &eError, vr::VRApplication_Scene );

	if ( eError != vr::VRInitError_None )
	{
		m_pHMD = NULL;
		char buf[1024];
		sprintf_s( buf, sizeof( buf ), "Unable to init VR runtime: %s", vr::VR_GetVRInitErrorAsEnglishDescription( eError ) );
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "VR_Init Failed", buf, NULL );
		return false;
	}


	int nWindowPosX = 700;
	int nWindowPosY = 100;
	Uint32 unWindowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;

	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 4 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 1 );
	//SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );

	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );
	if( m_bDebugOpenGL )
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );

	m_pCompanionWindow = SDL_CreateWindow( "hellovr", nWindowPosX, nWindowPosY, m_nCompanionWindowWidth, m_nCompanionWindowHeight, unWindowFlags );
	if (m_pCompanionWindow == NULL)
	{
		printf( "%s - Window could not be created! SDL Error: %s\n", __FUNCTION__, SDL_GetError() );
		return false;
	}

	m_pContext = SDL_GL_CreateContext(m_pCompanionWindow);
	if (m_pContext == NULL)
	{
		printf( "%s - OpenGL context could not be created! SDL Error: %s\n", __FUNCTION__, SDL_GetError() );
		return false;
	}

	glewExperimental = GL_TRUE;
	GLenum nGlewError = glewInit();
	if (nGlewError != GLEW_OK)
	{
		printf( "%s - Error initializing GLEW! %s\n", __FUNCTION__, glewGetErrorString( nGlewError ) );
		return false;
	}
	glGetError(); // to clear the error caused deep in GLEW

	if ( SDL_GL_SetSwapInterval( m_bVblank ? 1 : 0 ) < 0 )
	{
		printf( "%s - Warning: Unable to set VSync! SDL Error: %s\n", __FUNCTION__, SDL_GetError() );
		return false;
	}


	m_strDriver = "No Driver";
	m_strDisplay = "No Display";

	m_strDriver = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String );
	m_strDisplay = GetTrackedDeviceString( vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String );

	std::string strWindowTitle = "hellovr - " + m_strDriver + " " + m_strDisplay;
	SDL_SetWindowTitle( m_pCompanionWindow, strWindowTitle.c_str() );
	
	// cube array
 	m_iSceneVolumeWidth = m_iSceneVolumeInit;
 	m_iSceneVolumeHeight = m_iSceneVolumeInit;
 	m_iSceneVolumeDepth = m_iSceneVolumeInit;
 		
 	m_fScale = 0.3f;
 	m_fScaleSpacing = 4.0f;
 
 	m_fNearClip = 0.1f;
 	m_fFarClip = 30.0f;
 
 	m_iTexture = 0;
 	m_uiVertcount = 0;
 
// 		m_MillisecondsTimer.start(1, this);
// 		m_SecondsTimer.start(1000, this);
	
	if (!BInitGL())
	{
		printf("%s - Unable to initialize OpenGL!\n", __FUNCTION__);
		return false;
	}

	if (!BInitCompositor())
	{
		printf("%s - Failed to initialize VR Compositor!\n", __FUNCTION__);
		return false;
	}

    vr::VRInput()->SetActionManifestPath( m_strActionManifestPath.c_str() );

    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/in/ResetGame", &m_actionResetGame );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up ResetGame, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/in/MoveWorld", &m_actionMoveWorld );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up MoveWorld, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/in/TwistCommand", &m_actionTwistCommand );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up TwistCommand, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/in/TriggerHaptic", &m_actionTriggerHaptic );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up TriggerHaptic, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/in/AnalogInput", &m_actionAnalongInput );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up AnalogInput, err=" << int(err) << std::endl;};}

    {vr::EVRInputError err=vr::VRInput()->GetActionSetHandle( "/actions/vrviz", &m_actionsetVrviz );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up vrviz, err=" << int(err) << std::endl;};}

    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/out/Haptic_Left", &m_rHand[Left].m_actionHaptic );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up Haptic_Left, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/out/Haptic_Right", &m_rHand[Right].m_actionHaptic );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up Haptic_Right, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetInputSourceHandle( "/user/hand/left", &m_rHand[Left].m_source );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up left, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetInputSourceHandle( "/user/hand/right", &m_rHand[Right].m_source );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up right, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/in/Hand_Left", &m_rHand[Left].m_actionPose );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up Hand_Left, err=" << int(err) << std::endl;};}
    {vr::EVRInputError err=vr::VRInput()->GetActionHandle( "/actions/vrviz/in/Hand_Right", &m_rHand[Right].m_actionPose );if(err!=vr::EVRInputError::VRInputError_None){std::cout << "Error setting up Hand_Right, err=" << int(err) << std::endl;};}

    /*
    vr::VRInput()->GetActionHandle( "/actions/vrviz/in/ResetGame", &m_actionResetGame );
    vr::VRInput()->GetActionHandle( "/actions/vrviz/in/MoveWorld", &m_actionMoveWorld );
    vr::VRInput()->GetActionHandle( "/actions/vrviz/in/TwistCommand", &m_actionTwistCommand );
    //vr::VRInput()->GetActionHandle( "/actions/vrviz/in/TriggerHaptic", &m_actionTriggerHaptic );
    vr::VRInput()->GetActionHandle( "/actions/vrviz/in/AnalogInput", &m_actionAnalongInput );

    vr::VRInput()->GetActionSetHandle( "/actions/vrviz", &m_actionsetVrviz );

    vr::VRInput()->GetActionHandle( "/actions/vrviz/out/Haptic_Left", &m_rHand[Left].m_actionHaptic );
	vr::VRInput()->GetInputSourceHandle( "/user/hand/left", &m_rHand[Left].m_source );
    vr::VRInput()->GetActionHandle( "/actions/vrviz/in/Hand_Left", &m_rHand[Left].m_actionPose );

    vr::VRInput()->GetActionHandle( "/actions/vrviz/out/Haptic_Right", &m_rHand[Right].m_actionHaptic );
	vr::VRInput()->GetInputSourceHandle( "/user/hand/right", &m_rHand[Right].m_source );
    vr::VRInput()->GetActionHandle( "/actions/vrviz/in/Hand_Right", &m_rHand[Right].m_actionPose );
    */

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Outputs the string in message to debugging output.
//          All other parameters are ignored.
//          Does not return any meaningful value or reference.
//-----------------------------------------------------------------------------
void APIENTRY DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, const void* userParam)
{
	dprintf( "GL Error: %s\n", message );
}


//-----------------------------------------------------------------------------
// Purpose: Initialize OpenGL. Returns true if OpenGL has been successfully
//          initialized, false if shaders could not be created.
//          If failure occurred in a module other than shaders, the function
//          may return true or throw an error. 
//-----------------------------------------------------------------------------
bool CMainApplication::BInitGL()
{
	if( m_bDebugOpenGL )
	{
		glDebugMessageCallback( (GLDEBUGPROC)DebugCallback, nullptr);
		glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE );
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}

	if( !CreateAllShaders() )
		return false;

	SetupTexturemaps();
	SetupScene();
	SetupCameras();
	SetupStereoRenderTargets();
	SetupCompanionWindow();

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Initialize Compositor. Returns true if the compositor was
//          successfully initialized, false otherwise.
//-----------------------------------------------------------------------------
bool CMainApplication::BInitCompositor()
{
	vr::EVRInitError peError = vr::VRInitError_None;

	if ( !vr::VRCompositor() )
	{
		printf( "Compositor initialization failed. See log file for details\n" );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::Shutdown()
{
	if( m_pHMD )
	{
		vr::VR_Shutdown();
		m_pHMD = NULL;
	}

	for( std::vector< CGLRenderModel * >::iterator i = m_vecRenderModels.begin(); i != m_vecRenderModels.end(); i++ )
	{
		delete (*i);
	}
	m_vecRenderModels.clear();
	
	if( m_pContext )
	{
		if( m_bDebugOpenGL )
		{
			glDebugMessageControl( GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE );
			glDebugMessageCallback(nullptr, nullptr);
		}
		glDeleteBuffers(1, &m_glSceneVertBuffer);

		if ( m_unSceneProgramID )
		{
			glDeleteProgram( m_unSceneProgramID );
		}
		if ( m_unControllerTransformProgramID )
		{
			glDeleteProgram( m_unControllerTransformProgramID );
		}
		if ( m_unRenderModelProgramID )
		{
			glDeleteProgram( m_unRenderModelProgramID );
		}
		if ( m_unCompanionWindowProgramID )
		{
			glDeleteProgram( m_unCompanionWindowProgramID );
		}

		glDeleteRenderbuffers( 1, &leftEyeDesc.m_nDepthBufferId );
		glDeleteTextures( 1, &leftEyeDesc.m_nRenderTextureId );
		glDeleteFramebuffers( 1, &leftEyeDesc.m_nRenderFramebufferId );
		glDeleteTextures( 1, &leftEyeDesc.m_nResolveTextureId );
		glDeleteFramebuffers( 1, &leftEyeDesc.m_nResolveFramebufferId );

		glDeleteRenderbuffers( 1, &rightEyeDesc.m_nDepthBufferId );
		glDeleteTextures( 1, &rightEyeDesc.m_nRenderTextureId );
		glDeleteFramebuffers( 1, &rightEyeDesc.m_nRenderFramebufferId );
		glDeleteTextures( 1, &rightEyeDesc.m_nResolveTextureId );
		glDeleteFramebuffers( 1, &rightEyeDesc.m_nResolveFramebufferId );

		if( m_unCompanionWindowVAO != 0 )
		{
			glDeleteVertexArrays( 1, &m_unCompanionWindowVAO );
		}
		if( m_unSceneVAO != 0 )
		{
			glDeleteVertexArrays( 1, &m_unSceneVAO );
		}
		if( m_unControllerVAO != 0 )
		{
			glDeleteVertexArrays( 1, &m_unControllerVAO );
		}
		if( m_unPointCloudVAO != 0 ){
			glDeleteVertexArrays( 1, &m_unPointCloudVAO );
		}
		if( m_unColorTrisVAO != 0 ){
			glDeleteVertexArrays( 1, &m_unColorTrisVAO );
		}
	}

	if( m_pCompanionWindow )
	{
		SDL_DestroyWindow(m_pCompanionWindow);
		m_pCompanionWindow = NULL;
	}

	SDL_Quit();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMainApplication::HandleInput()
{
	SDL_Event sdlEvent;
	bool bRet = false;

	while ( SDL_PollEvent( &sdlEvent ) != 0 )
	{
		if ( sdlEvent.type == SDL_QUIT )
		{
			bRet = true;
		}
		else if ( sdlEvent.type == SDL_KEYDOWN )
		{
			if ( sdlEvent.key.keysym.sym == SDLK_ESCAPE 
			     || sdlEvent.key.keysym.sym == SDLK_q )
			{
				bRet = true;
			}
			if( sdlEvent.key.keysym.sym == SDLK_c )
			{
				m_bShowCubes = !m_bShowCubes;
			}
		}
	}

	// Process SteamVR events
	vr::VREvent_t event;
	while( m_pHMD->PollNextEvent( &event, sizeof( event ) ) )
	{
		ProcessVREvent( event );
	}

	// Process SteamVR action state
	// UpdateActionState is called each frame to update the state of the actions themselves. The application
	// controls which action sets are active with the provided array of VRActiveActionSet_t structs.
	vr::VRActiveActionSet_t actionSet = { 0 };
    actionSet.ulActionSet = m_actionsetVrviz;
	vr::VRInput()->UpdateActionState( &actionSet, sizeof(actionSet), 1 );

    //m_bShowCubes = !GetDigitalActionState( m_actionHideCubes );

	vr::VRInputValueHandle_t ulHapticDevice;
	if ( GetDigitalActionRisingEdge( m_actionTriggerHaptic, &ulHapticDevice ) )
	{
		if ( ulHapticDevice == m_rHand[Left].m_source )
		{
			vr::VRInput()->TriggerHapticVibrationAction( m_rHand[Left].m_actionHaptic, 0, 1, 4.f, 1.0f, vr::k_ulInvalidInputValueHandle );
		}
		if ( ulHapticDevice == m_rHand[Right].m_source )
		{
			vr::VRInput()->TriggerHapticVibrationAction( m_rHand[Right].m_actionHaptic, 0, 1, 4.f, 1.0f, vr::k_ulInvalidInputValueHandle );
		}
	}

	vr::InputAnalogActionData_t analogData;
	if ( vr::VRInput()->GetAnalogActionData( m_actionAnalongInput, &analogData, sizeof( analogData ), vr::k_ulInvalidInputValueHandle ) == vr::VRInputError_None && analogData.bActive )
	{
		m_vAnalogValue[0] = analogData.x;
		m_vAnalogValue[1] = analogData.y;
	}

    m_rHand[Left].m_bShowController = m_bShowControllers;
    m_rHand[Right].m_bShowController = m_bShowControllers;

//	vr::VRInputValueHandle_t ulHideDevice;
//	if ( GetDigitalActionState( m_actionHideThisController, &ulHideDevice ) )
//	{
//		if ( ulHideDevice == m_rHand[Left].m_source )
//		{
//			m_rHand[Left].m_bShowController = false;
//		}
//		if ( ulHideDevice == m_rHand[Right].m_source )
//		{
//			m_rHand[Right].m_bShowController = false;
//		}
//	}

	for ( EHand eHand = Left; eHand <= Right; ((int&)eHand)++ )
	{
		vr::InputPoseActionData_t poseData;

        vr::EVRInputError err=vr::VRInput()->GetPoseActionData( m_rHand[eHand].m_actionPose, vr::TrackingUniverseStanding, 0, &poseData, sizeof( poseData ), vr::k_ulInvalidInputValueHandle );

        if ( err != vr::VRInputError_None
			|| !poseData.bActive || !poseData.pose.bPoseIsValid )
		{
			m_rHand[eHand].m_bShowController = false;
            //std::cout << "Error getting pose for controller " << eHand << ", err=" << int(err) << ", Active="<< poseData.bActive << ", PoseValid=" << poseData.pose.bPoseIsValid << std::endl;
		}
		else
		{
			m_rHand[eHand].m_rmat4Pose = ConvertSteamVRMatrixToMatrix4( poseData.pose.mDeviceToAbsoluteTracking );

			vr::InputOriginInfo_t originInfo;
			if ( vr::VRInput()->GetOriginTrackedDeviceInfo( poseData.activeOrigin, &originInfo, sizeof( originInfo ) ) == vr::VRInputError_None 
				&& originInfo.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid )
			{
				std::string sRenderModelName = GetTrackedDeviceString( originInfo.trackedDeviceIndex, vr::Prop_RenderModelName_String );
				if ( sRenderModelName != m_rHand[eHand].m_sRenderModelName )
				{
					m_rHand[eHand].m_pRenderModel = FindOrLoadRenderModel( sRenderModelName.c_str() );
					m_rHand[eHand].m_sRenderModelName = sRenderModelName;
				}
			}
		}
	}

	return bRet;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::RunMainLoop()
{
	bool bQuit = false;

	SDL_StartTextInput();
	SDL_ShowCursor( SDL_DISABLE );

	while ( !bQuit )
	{
		bQuit = HandleInput();

		RenderFrame();
	}

	SDL_StopTextInput();
}


//-----------------------------------------------------------------------------
// Purpose: Processes a single VR event
//-----------------------------------------------------------------------------
void CMainApplication::ProcessVREvent( const vr::VREvent_t & event )
{
	switch( event.eventType )
	{
	case vr::VREvent_TrackedDeviceDeactivated:
		{
			dprintf( "Device %u detached.\n", event.trackedDeviceIndex );
		}
		break;
	case vr::VREvent_TrackedDeviceUpdated:
		{
			dprintf( "Device %u updated.\n", event.trackedDeviceIndex );
		}
		break;
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::RenderFrame()
{
	// for now as fast as possible
	if ( m_pHMD )
	{
		RenderControllerAxes();
		RenderStereoTargets();
		RenderCompanionWindow();

		vr::Texture_t leftEyeTexture = {(void*)(uintptr_t)leftEyeDesc.m_nResolveTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture );
		vr::Texture_t rightEyeTexture = {(void*)(uintptr_t)rightEyeDesc.m_nResolveTextureId, vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
		vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture );
	}

	if ( m_bVblank && m_bGlFinishHack )
	{
		//$ HACKHACK. From gpuview profiling, it looks like there is a bug where two renders and a present
		// happen right before and after the vsync causing all kinds of jittering issues. This glFinish()
		// appears to clear that up. Temporary fix while I try to get nvidia to investigate this problem.
		// 1/29/2014 mikesart
		glFinish();
	}

	// SwapWindow
	{
		SDL_GL_SwapWindow( m_pCompanionWindow );
	}

	// Clear
	{
		// We want to make sure the glFinish waits for the entire present to complete, not just the submission
		// of the command. So, we do a clear here right here so the glFinish will wait fully for the swap.
		glClearColor( 0, 0, 0, 1 );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	// Flush and wait for swap.
	if ( m_bVblank )
	{
		glFlush();
		glFinish();
	}

	// Spew out the controller and pose count whenever they change.
	if ( m_iTrackedControllerCount != m_iTrackedControllerCount_Last || m_iValidPoseCount != m_iValidPoseCount_Last )
	{
		m_iValidPoseCount_Last = m_iValidPoseCount;
		m_iTrackedControllerCount_Last = m_iTrackedControllerCount;
		
		//dprintf( "PoseCount:%d(%s) Controllers:%d\n", m_iValidPoseCount, m_strPoseClasses.c_str(), m_iTrackedControllerCount );
	}

	UpdateHMDMatrixPose();
}


//-----------------------------------------------------------------------------
// Purpose: Compiles a GL shader program and returns the handle. Returns 0 if
//			the shader couldn't be compiled for some reason.
//-----------------------------------------------------------------------------
GLuint CMainApplication::CompileGLShader( const char *pchShaderName, const char *pchVertexShader, const char *pchFragmentShader )
{
	GLuint unProgramID = glCreateProgram();

	GLuint nSceneVertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource( nSceneVertexShader, 1, &pchVertexShader, NULL);
	glCompileShader( nSceneVertexShader );

	GLint vShaderCompiled = GL_FALSE;
	glGetShaderiv( nSceneVertexShader, GL_COMPILE_STATUS, &vShaderCompiled);
	if ( vShaderCompiled != GL_TRUE)
	{
		dprintf("%s - Unable to compile vertex shader %d!\n", pchShaderName, nSceneVertexShader);
		glDeleteProgram( unProgramID );
		glDeleteShader( nSceneVertexShader );
		return 0;
	}
	glAttachShader( unProgramID, nSceneVertexShader);
	glDeleteShader( nSceneVertexShader ); // the program hangs onto this once it's attached

	GLuint  nSceneFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource( nSceneFragmentShader, 1, &pchFragmentShader, NULL);
	glCompileShader( nSceneFragmentShader );

	GLint fShaderCompiled = GL_FALSE;
	glGetShaderiv( nSceneFragmentShader, GL_COMPILE_STATUS, &fShaderCompiled);
	if (fShaderCompiled != GL_TRUE)
	{
		dprintf("%s - Unable to compile fragment shader %d!\n", pchShaderName, nSceneFragmentShader );
		glDeleteProgram( unProgramID );
		glDeleteShader( nSceneFragmentShader );
		return 0;	
	}

	glAttachShader( unProgramID, nSceneFragmentShader );
	glDeleteShader( nSceneFragmentShader ); // the program hangs onto this once it's attached

	glLinkProgram( unProgramID );

	GLint programSuccess = GL_TRUE;
	glGetProgramiv( unProgramID, GL_LINK_STATUS, &programSuccess);
	if ( programSuccess != GL_TRUE )
	{
		dprintf("%s - Error linking program %d!\n", pchShaderName, unProgramID);
		glDeleteProgram( unProgramID );
		return 0;
	}

	glUseProgram( unProgramID );
	glUseProgram( 0 );

	return unProgramID;
}


//-----------------------------------------------------------------------------
// Purpose: Creates all the shaders used by HelloVR SDL
//-----------------------------------------------------------------------------
bool CMainApplication::CreateAllShaders()
{
	m_unSceneProgramID = CompileGLShader( 
		"Scene",

		// Vertex Shader
		"#version 410\n"
		"uniform mat4 matrix;\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec2 v2UVcoordsIn;\n"
		"layout(location = 2) in vec3 v3NormalIn;\n"
		"out vec2 v2UVcoords;\n"
		"void main()\n"
		"{\n"
		"	v2UVcoords = v2UVcoordsIn;\n"
		"	gl_Position = matrix * position;\n"
		"}\n",

		// Fragment Shader
		"#version 410 core\n"
		"uniform sampler2D mytexture;\n"
		"in vec2 v2UVcoords;\n"
		"out vec4 outputColor;\n"
		"void main()\n"
		"{\n"
		"   outputColor = texture(mytexture, v2UVcoords);\n"
		"}\n"
		);
	m_nSceneMatrixLocation = glGetUniformLocation( m_unSceneProgramID, "matrix" );
	if( m_nSceneMatrixLocation == -1 )
	{
		dprintf( "Unable to find matrix uniform in scene shader\n" );
		return false;
	}

	m_unControllerTransformProgramID = CompileGLShader(
		"Controller",

		// vertex shader
		"#version 410\n"
		"uniform mat4 matrix;\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec3 v3ColorIn;\n"
		"out vec4 v4Color;\n"
		"void main()\n"
		"{\n"
		"	v4Color.xyz = v3ColorIn; v4Color.a = 1.0;\n"
		"	gl_Position = matrix * position;\n"
		"}\n",

		// fragment shader
		"#version 410\n"
		"in vec4 v4Color;\n"
		"out vec4 outputColor;\n"
		"void main()\n"
		"{\n"
		"   outputColor = v4Color;\n"
		"}\n"
		);
	m_nControllerMatrixLocation = glGetUniformLocation( m_unControllerTransformProgramID, "matrix" );
	if( m_nControllerMatrixLocation == -1 )
	{
		dprintf( "Unable to find matrix uniform in controller shader\n" );
		return false;
	}



    m_unRenderModelProgramID = CompileGLShader(
        "render model",

        // vertex shader
        "#version 410\n"
        "uniform mat4 matrix;\n"
        "layout(location = 0) in vec4 position;\n"
        "layout(location = 1) in vec3 v3NormalIn;\n"
        "layout(location = 2) in vec2 v2TexCoordsIn;\n"
        "out vec2 v2TexCoord;\n"
        "void main()\n"
        "{\n"
        "	v2TexCoord = v2TexCoordsIn;\n"
        "	gl_Position = matrix * vec4(position.xyz, 1);\n"
        "}\n",

        //fragment shader
        "#version 410 core\n"
        "uniform sampler2D diffuse;\n"
        "in vec2 v2TexCoord;\n"
        "out vec4 outputColor;\n"
        "void main()\n"
        "{\n"
        "   outputColor = texture( diffuse, v2TexCoord);\n"
        "}\n"

        );
    m_nRenderModelMatrixLocation = glGetUniformLocation( m_unRenderModelProgramID, "matrix" );
    if( m_nRenderModelMatrixLocation == -1 )
    {
        dprintf( "Unable to find matrix uniform in render model shader\n" );
        return false;
    }

    m_unLitRGBModelProgramID = CompileGLShader(
		"render model",

		// vertex shader
		"#version 330\n"
		"\n"
		"layout (location = 0) in vec3 Position;\n"
		"layout (location = 1) in vec3 Normal;\n"
		"layout (location = 2) in vec3 v3ColorIn;\n"
		"\n"
		"uniform mat4 gWVP;\n"
		"uniform mat4 gWorld;\n"
		"\n"
		"out vec4 v4Color;\n"
		"out vec3 Normal0;\n"
		"out vec3 WorldPos0;\n"
		"\n"
		"void main()\n"
		"{\n"
		" gl_Position = gWVP * vec4(Position, 1.0);\n"
		" v4Color = vec4(v3ColorIn, 1.0);\n"
		" Normal0 = (gWorld * vec4(Normal, 0.0)).xyz;\n"
		" WorldPos0 = (gWorld * vec4(Position, 1.0)).xyz;\n"
		"}\n",

		//fragment shader
		"#version 330\n"
		"\n"
		"const int MAX_POINT_LIGHTS = 2;\n"
		"const int MAX_SPOT_LIGHTS = 2;\n"
		"\n"
		"in vec4 v4Color;\n"
		"in vec3 Normal0;\n"
		"in vec3 WorldPos0;\n"
		"\n"
		"out vec4 FragColor;\n"
		"\n"
		"struct BaseLight\n"
		"{\n"
		" vec3 Color;\n"
		" float AmbientIntensity;\n"
		" float DiffuseIntensity;\n"
		"};\n"
		"\n"
		"struct DirectionalLight\n"
		"{\n"
		" BaseLight Base;\n"
		" vec3 Direction;\n"
		"};\n"
		"\n"
		"struct Attenuation\n"
		"{\n"
		" float Constant;\n"
		" float Linear;\n"
		" float Exp;\n"
		"};\n"
		"\n"
		"struct PointLight\n"
		"{\n"
		" BaseLight Base;\n"
		" vec3 Position;\n"
		" Attenuation Atten;\n"
		"};\n"
		"\n"
		"struct SpotLight\n"
		"{\n"
		" PointLight Base;\n"
		" vec3 Direction;\n"
		" float Cutoff;\n"
		"};\n"
		"\n"
		"uniform int gNumPointLights;\n"
		"uniform int gNumSpotLights;\n"
		"uniform DirectionalLight gDirectionalLight;\n"
		"uniform PointLight gPointLights[MAX_POINT_LIGHTS];\n"
		"uniform SpotLight gSpotLights[MAX_SPOT_LIGHTS];\n"
		"uniform sampler2D gColorMap;\n"
		"uniform vec3 gEyeWorldPos;\n"
		"uniform float gMatSpecularIntensity;\n"
		"uniform float gSpecularPower;\n"
		"\n"
		"vec4 CalcLightInternal(BaseLight Light, vec3 LightDirection, vec3 Normal)\n"
		"{\n"
		" vec4 AmbientColor = vec4(Light.Color * Light.AmbientIntensity, 1.0f);\n"
		" float DiffuseFactor = dot(Normal, -LightDirection);\n"
		"\n"
		" vec4 DiffuseColor = vec4(0, 0, 0, 0);\n"
		" vec4 SpecularColor = vec4(0, 0, 0, 0);\n"
		"\n"
		" if (DiffuseFactor > 0) {\n"
		" DiffuseColor = vec4(Light.Color * Light.DiffuseIntensity * DiffuseFactor, 1.0f);\n"
		"\n"
		" vec3 VertexToEye = normalize(gEyeWorldPos - WorldPos0);\n"
		" vec3 LightReflect = normalize(reflect(LightDirection, Normal));\n"
		" float SpecularFactor = dot(VertexToEye, LightReflect);\n"
		" if (SpecularFactor > 0) {\n"
		" SpecularFactor = pow(SpecularFactor, gSpecularPower);\n"
		" SpecularColor = vec4(Light.Color * gMatSpecularIntensity * SpecularFactor, 1.0f);\n"
		" }\n"
		" }\n"
		"\n"
		" return (AmbientColor + DiffuseColor + SpecularColor);\n"
		"}\n"
		"\n"
		"vec4 CalcDirectionalLight(vec3 Normal)\n"
		"{\n"
		" return CalcLightInternal(gDirectionalLight.Base, gDirectionalLight.Direction, Normal);\n"
		"}\n"
		"\n"
		"vec4 CalcPointLight(PointLight l, vec3 Normal)\n"
		"{\n"
		" vec3 LightDirection = WorldPos0 - l.Position;\n"
		" float Distance = length(LightDirection);\n"
		" LightDirection = normalize(LightDirection);\n"
		"\n"
		" vec4 Color = CalcLightInternal(l.Base, LightDirection, Normal);\n"
		" float Attenuation = l.Atten.Constant +\n"
		" l.Atten.Linear * Distance +\n"
		" l.Atten.Exp * Distance * Distance;\n"
		"\n"
		" return Color / Attenuation;\n"
		"}\n"
		"\n"
		"vec4 CalcSpotLight(SpotLight l, vec3 Normal)\n"
		"{\n"
		" vec3 LightToPixel = normalize(WorldPos0 - l.Base.Position);\n"
		" float SpotFactor = dot(LightToPixel, l.Direction);\n"
		"\n"
		" if (SpotFactor > l.Cutoff) {\n"
		" vec4 Color = CalcPointLight(l.Base, Normal);\n"
		" return Color * (1.0 - (1.0 - SpotFactor) * 1.0/(1.0 - l.Cutoff));\n"
		" }\n"
		" else {\n"
		" return vec4(0,0,0,0);\n"
		" }\n"
		"}\n"
		"\n"
		"void main()\n"
		"{\n"
		" vec3 Normal = normalize(Normal0);\n"
		" vec4 TotalLight = CalcDirectionalLight(Normal);\n"
		"\n"
		" for (int i = 0 ; i < gNumPointLights ; i++) {\n"
		" TotalLight += CalcPointLight(gPointLights[i], Normal);\n"
		" }\n"
		"\n"
		" for (int i = 0 ; i < gNumSpotLights ; i++) {\n"
		" TotalLight += CalcSpotLight(gSpotLights[i], Normal);\n"
		" }\n"
		"\n"
		" FragColor = v4Color * TotalLight;\n"
		"}\n"

		);

    m_nLitRGBModelMatrixLocation = glGetUniformLocation( m_unLitRGBModelProgramID, "gWVP");
    m_WorldMatrixRGBLocation = glGetUniformLocation( m_unLitRGBModelProgramID, "gWorld");
    m_colorTextureRGBLocation = glGetUniformLocation( m_unLitRGBModelProgramID, "gColorMap");
    m_eyeWorldPosRGBLocation = glGetUniformLocation( m_unLitRGBModelProgramID, "gEyeWorldPos");
    m_dirLightRGBLocation.Color = glGetUniformLocation( m_unLitRGBModelProgramID, "gDirectionalLight.Base.Color");
    m_dirLightRGBLocation.AmbientIntensity = glGetUniformLocation( m_unLitRGBModelProgramID, "gDirectionalLight.Base.AmbientIntensity");
    m_dirLightRGBLocation.Direction = glGetUniformLocation( m_unLitRGBModelProgramID, "gDirectionalLight.Direction");
    m_dirLightRGBLocation.DiffuseIntensity = glGetUniformLocation( m_unLitRGBModelProgramID, "gDirectionalLight.Base.DiffuseIntensity");
    m_matSpecularIntensityRGBLocation = glGetUniformLocation( m_unLitRGBModelProgramID, "gMatSpecularIntensity");
    m_matSpecularPowerRGBLocation = glGetUniformLocation( m_unLitRGBModelProgramID, "gSpecularPower");
    m_numPointLightsRGBLocation = glGetUniformLocation( m_unLitRGBModelProgramID, "gNumPointLights");
    m_numSpotLightsRGBLocation = glGetUniformLocation( m_unLitRGBModelProgramID, "gNumSpotLights");



    if( m_nLitRGBModelMatrixLocation == -1 )
	{
		dprintf( "Unable to find matrix uniform in render rgb model shader\n" );
		return false;
	}




    m_unLitModelProgramID = CompileGLShader(
		"render model",

		// vertex shader
		"#version 330\n"
		"\n"
		"layout (location = 0) in vec3 Position;\n"
		"layout (location = 1) in vec3\n"
		"Normal;\n"
		"layout (location = 2) in vec2 TexCoord;\n"
		"\n"
		"uniform mat4 gWVP;\n"
		"uniform mat4 gWorld;\n"
		"\n"
		"out vec2 TexCoord0;\n"
		"out vec3 Normal0;\n"
		"out vec3 WorldPos0;\n"
		"\n"
		"void main()\n"
		"{\n"
		" gl_Position = gWVP * vec4(Position, 1.0);\n"
		" TexCoord0 = TexCoord;\n"
		" Normal0 = (gWorld * vec4(Normal, 0.0)).xyz;\n"
		" WorldPos0 = (gWorld * vec4(Position, 1.0)).xyz;\n"
		"}\n",

		//fragment shader
		"#version 330\n"
		"\n"
		"const int MAX_POINT_LIGHTS = 2;\n"
		"const int MAX_SPOT_LIGHTS = 2;\n"
		"\n"
		"in vec2 TexCoord0;\n"
		"in vec3 Normal0;\n"
		"in vec3 WorldPos0;\n"
		"\n"
		"out vec4 FragColor;\n"
		"\n"
		"struct BaseLight\n"
		"{\n"
		" vec3 Color;\n"
		" float AmbientIntensity;\n"
		" float DiffuseIntensity;\n"
		"};\n"
		"\n"
		"struct DirectionalLight\n"
		"{\n"
		" BaseLight Base;\n"
		" vec3 Direction;\n"
		"};\n"
		"\n"
		"struct Attenuation\n"
		"{\n"
		" float Constant;\n"
		" float Linear;\n"
		" float Exp;\n"
		"};\n"
		"\n"
		"struct PointLight\n"
		"{\n"
		" BaseLight Base;\n"
		" vec3 Position;\n"
		" Attenuation Atten;\n"
		"};\n"
		"\n"
		"struct SpotLight\n"
		"{\n"
		" PointLight Base;\n"
		" vec3 Direction;\n"
		" float Cutoff;\n"
		"};\n"
		"\n"
		"uniform int gNumPointLights;\n"
		"uniform int gNumSpotLights;\n"
		"uniform DirectionalLight gDirectionalLight;\n"
		"uniform PointLight gPointLights[MAX_POINT_LIGHTS];\n"
		"uniform SpotLight gSpotLights[MAX_SPOT_LIGHTS];\n"
		"uniform sampler2D gColorMap;\n"
		"uniform vec3 gEyeWorldPos;\n"
		"uniform float gMatSpecularIntensity;\n"
		"uniform float gSpecularPower;\n"
		"\n"
		"vec4 CalcLightInternal(BaseLight Light, vec3 LightDirection, vec3 Normal)\n"
		"{\n"
		" vec4 AmbientColor = vec4(Light.Color * Light.AmbientIntensity, 1.0f);\n"
		" float DiffuseFactor = dot(Normal, -LightDirection);\n"
		"\n"
		" vec4 DiffuseColor = vec4(0, 0, 0, 0);\n"
		" vec4 SpecularColor = vec4(0, 0, 0, 0);\n"
		"\n"
		" if (DiffuseFactor > 0) {\n"
		" DiffuseColor = vec4(Light.Color * Light.DiffuseIntensity * DiffuseFactor, 1.0f);\n"
		"\n"
		" vec3 VertexToEye = normalize(gEyeWorldPos - WorldPos0);\n"
		" vec3 LightReflect = normalize(reflect(LightDirection, Normal));\n"
		" float SpecularFactor = dot(VertexToEye, LightReflect);\n"
		" if (SpecularFactor > 0) {\n"
		" SpecularFactor = pow(SpecularFactor, gSpecularPower);\n"
		" SpecularColor = vec4(Light.Color * gMatSpecularIntensity * SpecularFactor, 1.0f);\n"
		" }\n"
		" }\n"
		"\n"
		" return (AmbientColor + DiffuseColor + SpecularColor);\n"
		"}\n"
		"\n"
		"vec4 CalcDirectionalLight(vec3 Normal)\n"
		"{\n"
		" return CalcLightInternal(gDirectionalLight.Base, gDirectionalLight.Direction, Normal);\n"
		"}\n"
		"\n"
		"vec4 CalcPointLight(PointLight l, vec3 Normal)\n"
		"{\n"
		" vec3 LightDirection = WorldPos0 - l.Position;\n"
		" float Distance = length(LightDirection);\n"
		" LightDirection = normalize(LightDirection);\n"
		"\n"
		" vec4 Color = CalcLightInternal(l.Base, LightDirection, Normal);\n"
		" float Attenuation = l.Atten.Constant +\n"
		" l.Atten.Linear * Distance +\n"
		" l.Atten.Exp * Distance * Distance;\n"
		"\n"
		" return Color / Attenuation;\n"
		"}\n"
		"\n"
		"vec4 CalcSpotLight(SpotLight l, vec3 Normal)\n"
		"{\n"
		" vec3 LightToPixel = normalize(WorldPos0 - l.Base.Position);\n"
		" float SpotFactor = dot(LightToPixel, l.Direction);\n"
		"\n"
		" if (SpotFactor > l.Cutoff) {\n"
		" vec4 Color = CalcPointLight(l.Base, Normal);\n"
		" return Color * (1.0 - (1.0 - SpotFactor) * 1.0/(1.0 - l.Cutoff));\n"
		" }\n"
		" else {\n"
		" return vec4(0,0,0,0);\n"
		" }\n"
		"}\n"
		"\n"
		"void main()\n"
		"{\n"
		" vec3 Normal = normalize(Normal0);\n"
		" vec4 TotalLight = CalcDirectionalLight(Normal);\n"
		"\n"
		" for (int i = 0 ; i < gNumPointLights ; i++) {\n"
		" TotalLight += CalcPointLight(gPointLights[i], Normal);\n"
		" }\n"
		"\n"
		" for (int i = 0 ; i < gNumSpotLights ; i++) {\n"
		" TotalLight += CalcSpotLight(gSpotLights[i], Normal);\n"
		" }\n"
		"\n"
		" FragColor = texture(gColorMap, TexCoord0.xy) * TotalLight;\n"
		"}\n"

		);

    m_nLitModelMatrixLocation = glGetUniformLocation( m_unLitModelProgramID, "gWVP");
    m_WorldMatrixLocation = glGetUniformLocation( m_unLitModelProgramID, "gWorld");
    m_colorTextureLocation = glGetUniformLocation( m_unLitModelProgramID, "gColorMap");
    m_eyeWorldPosLocation = glGetUniformLocation( m_unLitModelProgramID, "gEyeWorldPos");
    m_dirLightLocation.Color = glGetUniformLocation( m_unLitModelProgramID, "gDirectionalLight.Base.Color");
    m_dirLightLocation.AmbientIntensity = glGetUniformLocation( m_unLitModelProgramID, "gDirectionalLight.Base.AmbientIntensity");
    m_dirLightLocation.Direction = glGetUniformLocation( m_unLitModelProgramID, "gDirectionalLight.Direction");
    m_dirLightLocation.DiffuseIntensity = glGetUniformLocation( m_unLitModelProgramID, "gDirectionalLight.Base.DiffuseIntensity");
    m_matSpecularIntensityLocation = glGetUniformLocation( m_unLitModelProgramID, "gMatSpecularIntensity");
    m_matSpecularPowerLocation = glGetUniformLocation( m_unLitModelProgramID, "gSpecularPower");
    m_numPointLightsLocation = glGetUniformLocation( m_unLitModelProgramID, "gNumPointLights");
    m_numSpotLightsLocation = glGetUniformLocation( m_unLitModelProgramID, "gNumSpotLights");



    if( m_nLitModelMatrixLocation == -1 )
	{
        dprintf( "Unable to find matrix uniform in lit model shader\n" );
		return false;
	}

	m_unCompanionWindowProgramID = CompileGLShader(
		"CompanionWindow",

		// vertex shader
		"#version 410 core\n"
		"layout(location = 0) in vec4 position;\n"
		"layout(location = 1) in vec2 v2UVIn;\n"
		"noperspective out vec2 v2UV;\n"
		"void main()\n"
		"{\n"
		"	v2UV = v2UVIn;\n"
		"	gl_Position = position;\n"
		"}\n",

		// fragment shader
		"#version 410 core\n"
		"uniform sampler2D mytexture;\n"
		"noperspective in vec2 v2UV;\n"
		"out vec4 outputColor;\n"
		"void main()\n"
		"{\n"
		"		outputColor = texture(mytexture, v2UV);\n"
		"}\n"
		);

	return m_unSceneProgramID != 0 
		&& m_unControllerTransformProgramID != 0
		&& m_unRenderModelProgramID != 0
		&& m_unCompanionWindowProgramID != 0;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMainApplication::SetupTexturemaps()
{
	
	std::vector<unsigned char> imageRGBA;
	unsigned nImageWidth, nImageHeight;
	unsigned nError = lodepng::decode( imageRGBA, nImageWidth, nImageHeight, m_strTextPath.c_str() );
	
	if ( nError != 0 )
		return false;

	glGenTextures(1, &m_iTexture );
	glBindTexture( GL_TEXTURE_2D, m_iTexture );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, nImageWidth, nImageHeight,
		0, GL_RGBA, GL_UNSIGNED_BYTE, &imageRGBA[0] );

	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );

	GLfloat fLargest;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest);
	 	
	glBindTexture( GL_TEXTURE_2D, 0 );

	return ( m_iTexture != 0 );
}


//-----------------------------------------------------------------------------
// Purpose: create a sea of cubes
//-----------------------------------------------------------------------------
void CMainApplication::SetupScene()
{
	if ( !m_pHMD )
		return;

	std::vector<float> vertdataarray;

	Matrix4 matScale;
	matScale.scale( m_fScale, m_fScale, m_fScale );
	Matrix4 matTransform;
	matTransform.translate(
		-( (float)m_iSceneVolumeWidth * m_fScaleSpacing ) / 2.f,
		-( (float)m_iSceneVolumeHeight * m_fScaleSpacing ) / 2.f,
		-( (float)m_iSceneVolumeDepth * m_fScaleSpacing ) / 2.f);
	
	Matrix4 mat = matScale * matTransform;

	for( int z = 0; z< m_iSceneVolumeDepth; z++ )
	{
		for( int y = 0; y< m_iSceneVolumeHeight; y++ )
		{
			for( int x = 0; x< m_iSceneVolumeWidth; x++ )
			{
				AddCubeToScene( mat, vertdataarray );
				mat = mat * Matrix4().translate( m_fScaleSpacing, 0, 0 );
			}
			mat = mat * Matrix4().translate( -((float)m_iSceneVolumeWidth) * m_fScaleSpacing, m_fScaleSpacing, 0 );
		}
		mat = mat * Matrix4().translate( 0, -((float)m_iSceneVolumeHeight) * m_fScaleSpacing, m_fScaleSpacing );
	}
	m_uiVertcount = vertdataarray.size()/5;
	
	glGenVertexArrays( 1, &m_unSceneVAO );
	glBindVertexArray( m_unSceneVAO );

	glGenBuffers( 1, &m_glSceneVertBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, m_glSceneVertBuffer );
	glBufferData( GL_ARRAY_BUFFER, sizeof(float) * vertdataarray.size(), &vertdataarray[0], GL_STATIC_DRAW);

	GLsizei stride = sizeof(VertexDataScene);
	uintptr_t offset = 0;

	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, stride , (const void *)offset);

	offset += sizeof(Vector3);
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)offset);

	glBindVertexArray( 0 );
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::AddCubeVertex( float fl0, float fl1, float fl2, float fl3, float fl4, std::vector<float> &vertdata )
{
	vertdata.push_back( fl0 );
	vertdata.push_back( fl1 );
	vertdata.push_back( fl2 );
	vertdata.push_back( fl3 );
	vertdata.push_back( fl4 );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::AddCubeToScene( Matrix4 mat, std::vector<float> &vertdata )
{
	// Matrix4 mat( outermat.data() );

	Vector4 A = mat * Vector4( 0, 0, 0, 1 );
	Vector4 B = mat * Vector4( 1, 0, 0, 1 );
	Vector4 C = mat * Vector4( 1, 1, 0, 1 );
	Vector4 D = mat * Vector4( 0, 1, 0, 1 );
	Vector4 E = mat * Vector4( 0, 0, 1, 1 );
	Vector4 F = mat * Vector4( 1, 0, 1, 1 );
	Vector4 G = mat * Vector4( 1, 1, 1, 1 );
	Vector4 H = mat * Vector4( 0, 1, 1, 1 );

	// triangles instead of quads
	AddCubeVertex( E.x, E.y, E.z, 0, 1, vertdata ); //Front
	AddCubeVertex( F.x, F.y, F.z, 1, 1, vertdata );
	AddCubeVertex( G.x, G.y, G.z, 1, 0, vertdata );
	AddCubeVertex( G.x, G.y, G.z, 1, 0, vertdata );
	AddCubeVertex( H.x, H.y, H.z, 0, 0, vertdata );
	AddCubeVertex( E.x, E.y, E.z, 0, 1, vertdata );
					 
	AddCubeVertex( B.x, B.y, B.z, 0, 1, vertdata ); //Back
	AddCubeVertex( A.x, A.y, A.z, 1, 1, vertdata );
	AddCubeVertex( D.x, D.y, D.z, 1, 0, vertdata );
	AddCubeVertex( D.x, D.y, D.z, 1, 0, vertdata );
	AddCubeVertex( C.x, C.y, C.z, 0, 0, vertdata );
	AddCubeVertex( B.x, B.y, B.z, 0, 1, vertdata );
					
	AddCubeVertex( H.x, H.y, H.z, 0, 1, vertdata ); //Top
	AddCubeVertex( G.x, G.y, G.z, 1, 1, vertdata );
	AddCubeVertex( C.x, C.y, C.z, 1, 0, vertdata );
	AddCubeVertex( C.x, C.y, C.z, 1, 0, vertdata );
	AddCubeVertex( D.x, D.y, D.z, 0, 0, vertdata );
	AddCubeVertex( H.x, H.y, H.z, 0, 1, vertdata );
				
	AddCubeVertex( A.x, A.y, A.z, 0, 1, vertdata ); //Bottom
	AddCubeVertex( B.x, B.y, B.z, 1, 1, vertdata );
	AddCubeVertex( F.x, F.y, F.z, 1, 0, vertdata );
	AddCubeVertex( F.x, F.y, F.z, 1, 0, vertdata );
	AddCubeVertex( E.x, E.y, E.z, 0, 0, vertdata );
	AddCubeVertex( A.x, A.y, A.z, 0, 1, vertdata );
					
	AddCubeVertex( A.x, A.y, A.z, 0, 1, vertdata ); //Left
	AddCubeVertex( E.x, E.y, E.z, 1, 1, vertdata );
	AddCubeVertex( H.x, H.y, H.z, 1, 0, vertdata );
	AddCubeVertex( H.x, H.y, H.z, 1, 0, vertdata );
	AddCubeVertex( D.x, D.y, D.z, 0, 0, vertdata );
	AddCubeVertex( A.x, A.y, A.z, 0, 1, vertdata );

	AddCubeVertex( F.x, F.y, F.z, 0, 1, vertdata ); //Right
	AddCubeVertex( B.x, B.y, B.z, 1, 1, vertdata );
	AddCubeVertex( C.x, C.y, C.z, 1, 0, vertdata );
	AddCubeVertex( C.x, C.y, C.z, 1, 0, vertdata );
	AddCubeVertex( G.x, G.y, G.z, 0, 0, vertdata );
	AddCubeVertex( F.x, F.y, F.z, 0, 1, vertdata );
}


//-----------------------------------------------------------------------------
// Purpose: Draw all of the controllers as X/Y/Z lines
//-----------------------------------------------------------------------------
void CMainApplication::RenderControllerAxes()
{
	// Don't attempt to update controllers if input is not available
	if( !m_pHMD->IsInputAvailable() )
		return;

	std::vector<float> vertdataarray;

	m_uiControllerVertcount = 0;
	m_iTrackedControllerCount = 0;

	for ( EHand eHand = Left; eHand <= Right; ((int&)eHand)++ )
	{
		if ( !m_rHand[eHand].m_bShowController )
			continue;

		const Matrix4 & mat = m_rHand[eHand].m_rmat4Pose;

		Vector4 center = mat * Vector4( 0, 0, 0, 1 );

		for ( int i = 0; i < 3; ++i )
		{
			Vector3 color( 0, 0, 0 );
			Vector4 point( 0, 0, 0, 1 );
			point[i] += 0.05f;  // offset in X, Y, Z
			color[i] = 1.0;  // R, G, B
			point = mat * point;
			vertdataarray.push_back( center.x );
			vertdataarray.push_back( center.y );
			vertdataarray.push_back( center.z );

			vertdataarray.push_back( color.x );
			vertdataarray.push_back( color.y );
			vertdataarray.push_back( color.z );
		
			vertdataarray.push_back( point.x );
			vertdataarray.push_back( point.y );
			vertdataarray.push_back( point.z );
		
			vertdataarray.push_back( color.x );
			vertdataarray.push_back( color.y );
			vertdataarray.push_back( color.z );
		
			m_uiControllerVertcount += 2;
		}

		Vector4 start = mat * Vector4( 0, 0, -0.02f, 1 );
		Vector4 end = mat * Vector4( 0, 0, -39.f, 1 );
		Vector3 color( .92f, .92f, .71f );

		vertdataarray.push_back( start.x );vertdataarray.push_back( start.y );vertdataarray.push_back( start.z );
		vertdataarray.push_back( color.x );vertdataarray.push_back( color.y );vertdataarray.push_back( color.z );

		vertdataarray.push_back( end.x );vertdataarray.push_back( end.y );vertdataarray.push_back( end.z );
		vertdataarray.push_back( color.x );vertdataarray.push_back( color.y );vertdataarray.push_back( color.z );
		m_uiControllerVertcount += 2;
	}

	// Setup the VAO the first time through.
	if ( m_unControllerVAO == 0 )
	{
		glGenVertexArrays( 1, &m_unControllerVAO );
		glBindVertexArray( m_unControllerVAO );

		glGenBuffers( 1, &m_glControllerVertBuffer );
		glBindBuffer( GL_ARRAY_BUFFER, m_glControllerVertBuffer );

		GLuint stride = 2 * 3 * sizeof( float );
		uintptr_t offset = 0;

		glEnableVertexAttribArray( 0 );
		glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offset);

		offset += sizeof( Vector3 );
		glEnableVertexAttribArray( 1 );
		glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offset);

		glBindVertexArray( 0 );
	}

	glBindBuffer( GL_ARRAY_BUFFER, m_glControllerVertBuffer );

	// set vertex data if we have some
	if( vertdataarray.size() > 0 )
	{
		//$ TODO: Use glBufferSubData for this...
		glBufferData( GL_ARRAY_BUFFER, sizeof(float) * vertdataarray.size(), &vertdataarray[0], GL_STREAM_DRAW );
	}
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::SetupCameras()
{
	m_mat4ProjectionLeft = GetHMDMatrixProjectionEye( vr::Eye_Left );
	m_mat4ProjectionRight = GetHMDMatrixProjectionEye( vr::Eye_Right );
	m_mat4eyePosLeft = GetHMDMatrixPoseEye( vr::Eye_Left );
	m_mat4eyePosRight = GetHMDMatrixPoseEye( vr::Eye_Right );
}


//-----------------------------------------------------------------------------
// Purpose: Creates a frame buffer. Returns true if the buffer was set up.
//          Returns false if the setup failed.
//-----------------------------------------------------------------------------
bool CMainApplication::CreateFrameBuffer( int nWidth, int nHeight, FramebufferDesc &framebufferDesc )
{
	glGenFramebuffers(1, &framebufferDesc.m_nRenderFramebufferId );
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.m_nRenderFramebufferId);

	glGenRenderbuffers(1, &framebufferDesc.m_nDepthBufferId);
	glBindRenderbuffer(GL_RENDERBUFFER, framebufferDesc.m_nDepthBufferId);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT, nWidth, nHeight );
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,	framebufferDesc.m_nDepthBufferId );

	glGenTextures(1, &framebufferDesc.m_nRenderTextureId );
	glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc.m_nRenderTextureId );
	glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA8, nWidth, nHeight, true);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, framebufferDesc.m_nRenderTextureId, 0);

	glGenFramebuffers(1, &framebufferDesc.m_nResolveFramebufferId );
	glBindFramebuffer(GL_FRAMEBUFFER, framebufferDesc.m_nResolveFramebufferId);

	glGenTextures(1, &framebufferDesc.m_nResolveTextureId );
	glBindTexture(GL_TEXTURE_2D, framebufferDesc.m_nResolveTextureId );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, nWidth, nHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebufferDesc.m_nResolveTextureId, 0);

	// check FBO status
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		return false;
	}

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMainApplication::SetupStereoRenderTargets()
{
	if ( !m_pHMD )
		return false;

	m_pHMD->GetRecommendedRenderTargetSize( &m_nRenderWidth, &m_nRenderHeight );

	CreateFrameBuffer( m_nRenderWidth, m_nRenderHeight, leftEyeDesc );
	CreateFrameBuffer( m_nRenderWidth, m_nRenderHeight, rightEyeDesc );
	
	return true;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::SetupCompanionWindow()
{
	if ( !m_pHMD )
		return;

	std::vector<VertexDataWindow> vVerts;

	// left eye verts
	vVerts.push_back( VertexDataWindow( Vector2(-1, 1), Vector2(0, 1)) );
	vVerts.push_back( VertexDataWindow( Vector2(0, 1), Vector2(1, 1)) );
	vVerts.push_back( VertexDataWindow( Vector2(-1, -1), Vector2(0, 0)) );
	vVerts.push_back( VertexDataWindow( Vector2(0, -1), Vector2(1, 0)) );

	// right eye verts
	vVerts.push_back( VertexDataWindow( Vector2(0, 1), Vector2(0, 1)) );
	vVerts.push_back( VertexDataWindow( Vector2(1, 1), Vector2(1, 1)) );
	vVerts.push_back( VertexDataWindow( Vector2(0, -1), Vector2(0, 0)) );
	vVerts.push_back( VertexDataWindow( Vector2(1, -1), Vector2(1, 0)) );

	GLushort vIndices[] = { 0, 1, 3,   0, 3, 2,   4, 5, 7,   4, 7, 6};
	m_uiCompanionWindowIndexSize = _countof(vIndices);

	glGenVertexArrays( 1, &m_unCompanionWindowVAO );
	glBindVertexArray( m_unCompanionWindowVAO );

	glGenBuffers( 1, &m_glCompanionWindowIDVertBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, m_glCompanionWindowIDVertBuffer );
	glBufferData( GL_ARRAY_BUFFER, vVerts.size()*sizeof(VertexDataWindow), &vVerts[0], GL_STATIC_DRAW );

	glGenBuffers( 1, &m_glCompanionWindowIDIndexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_glCompanionWindowIDIndexBuffer );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, m_uiCompanionWindowIndexSize*sizeof(GLushort), &vIndices[0], GL_STATIC_DRAW );

	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexDataWindow), (void *)offsetof( VertexDataWindow, position ) );

	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexDataWindow), (void *)offsetof( VertexDataWindow, texCoord ) );

	glBindVertexArray( 0 );

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::RenderStereoTargets()
{
	glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	glEnable( GL_MULTISAMPLE );

	// Left Eye
	glBindFramebuffer( GL_FRAMEBUFFER, leftEyeDesc.m_nRenderFramebufferId );
 	glViewport(0, 0, m_nRenderWidth, m_nRenderHeight );
 	RenderScene( vr::Eye_Left );
 	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	
	glDisable( GL_MULTISAMPLE );
	 	
 	glBindFramebuffer(GL_READ_FRAMEBUFFER, leftEyeDesc.m_nRenderFramebufferId);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, leftEyeDesc.m_nResolveFramebufferId );

    glBlitFramebuffer( 0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight, 
		GL_COLOR_BUFFER_BIT,
 		GL_LINEAR );

 	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0 );	

	glEnable( GL_MULTISAMPLE );

	// Right Eye
	glBindFramebuffer( GL_FRAMEBUFFER, rightEyeDesc.m_nRenderFramebufferId );
 	glViewport(0, 0, m_nRenderWidth, m_nRenderHeight );
 	RenderScene( vr::Eye_Right );
 	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
 	
	glDisable( GL_MULTISAMPLE );

 	glBindFramebuffer(GL_READ_FRAMEBUFFER, rightEyeDesc.m_nRenderFramebufferId );
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rightEyeDesc.m_nResolveFramebufferId );
	
    glBlitFramebuffer( 0, 0, m_nRenderWidth, m_nRenderHeight, 0, 0, m_nRenderWidth, m_nRenderHeight, 
		GL_COLOR_BUFFER_BIT,
 		GL_LINEAR  );

 	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Renders a scene with respect to nEye.
//-----------------------------------------------------------------------------
void CMainApplication::RenderScene( vr::Hmd_Eye nEye )
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);

	if( m_bShowCubes )
	{
		glUseProgram( m_unSceneProgramID );
		glUniformMatrix4fv( m_nSceneMatrixLocation, 1, GL_FALSE, GetCurrentViewProjectionMatrix( nEye ).get() );
		glBindVertexArray( m_unSceneVAO );
		glBindTexture( GL_TEXTURE_2D, m_iTexture );
		glDrawArrays( GL_TRIANGLES, 0, m_uiVertcount );
		glBindVertexArray( 0 );
	}

	bool bIsInputAvailable = m_pHMD->IsInputAvailable();

	if( bIsInputAvailable )
	{
		// draw the controller axis lines
		glUseProgram( m_unControllerTransformProgramID );
		glUniformMatrix4fv( m_nControllerMatrixLocation, 1, GL_FALSE, GetCurrentViewProjectionMatrix( nEye ).get() );
		glBindVertexArray( m_unControllerVAO );
		glDrawArrays( GL_LINES, 0, m_uiControllerVertcount );
		glBindVertexArray( 0 );

        // Only bother drawing if there are points. This avoids calls to GetRobotMatrixPose() where m_strPointCloudFrame is an empty string.
        if(m_uiPointCloudVertcount>0){
            // draw the point cloud
            glUseProgram( m_unControllerTransformProgramID );
            glUniformMatrix4fv( m_nControllerMatrixLocation, 1, GL_FALSE, (GetCurrentViewProjectionMatrix( nEye ) * GetRobotMatrixPose(m_strPointCloudFrame)).get() );
            glBindVertexArray( m_unPointCloudVAO );
            glPointSize( m_unPointSize );
            glDrawArrays( GL_POINTS, 0, m_uiPointCloudVertcount );
            glBindVertexArray( 0 );
        }

		// draw the color triangle mesh
		glUseProgram( m_unControllerTransformProgramID );
		glUniformMatrix4fv( m_nControllerMatrixLocation, 1, GL_FALSE, GetCurrentViewProjectionMatrix( nEye ).get() );
		glBindVertexArray( m_unColorTrisVAO );
		glDrawArrays( GL_TRIANGLES, 0, m_uiColorTrisVertcount );
		glBindVertexArray( 0 );
	}

	// ----- Render Model rendering -----
	glUseProgram( m_unRenderModelProgramID );

	for ( EHand eHand = Left; eHand <= Right; ((int&)eHand)++ )
	{
		if ( !m_rHand[eHand].m_bShowController || !m_rHand[eHand].m_pRenderModel )
			continue;

		const Matrix4 & matDeviceToTracking = m_rHand[eHand].m_rmat4Pose;
		Matrix4 matMVP = GetCurrentViewProjectionMatrix( nEye ) * matDeviceToTracking;
		glUniformMatrix4fv( m_nRenderModelMatrixLocation, 1, GL_FALSE, matMVP.get() );

		m_rHand[eHand].m_pRenderModel->Draw();
	}


    for(int idx=0;idx<robot_meshes.size();idx++){

        //robot_meshes[idx]->Render();
        for(int jj=0;jj<robot_meshes[idx]->m_Entries.size();jj++){
            if(robot_meshes[idx]->initialized && !robot_meshes[idx]->load_mesh){
                if(robot_meshes[idx]->frame_id.length()==0)
                {
                    std::cout << "empty frameid when rendering " << robot_meshes[idx]->name << " ID=" << robot_meshes[idx]->id << " FrameID=" << robot_meshes[idx]->frame_id << std::endl;
                }

                if(robot_meshes[idx]->m_Entries[jj].MaterialIndex!=NO_TEXTURE){

                    // ----- Render Model rendering -----
                    glUseProgram( m_unLitModelProgramID );

                    Matrix4 matMVP = GetCurrentViewProjectionMatrix( nEye ) * GetRobotMatrixPose(robot_meshes[idx]->frame_id);
                    Matrix4 matWorld = GetRobotMatrixPose(robot_meshes[idx]->frame_id) ;
                    Vector4 eyePos = GetHMDMatrixPoseEye(nEye)*Vector4(0,0,0,1);
                    glUniformMatrix4fv( m_nLitModelMatrixLocation, 1, GL_FALSE, matMVP.get() );

                    /// The settings for light number, location, intensity, colour, etc could
                    /// all be made ros params so that they could be user settable.
                    glUniformMatrix4fv( m_WorldMatrixLocation, 1, GL_FALSE, matWorld.get() );
                    glUniform3f(m_eyeWorldPosLocation, eyePos.x,eyePos.y,eyePos.z);
                    glUniform1i(m_colorTextureLocation, 0);
                    glUniform3f(m_dirLightLocation.Color, 1.0,1.0,1.0);
                    glUniform1f(m_dirLightLocation.AmbientIntensity, 0.15);
                    glUniform3f(m_dirLightLocation.Direction, 0.0, -0.70710678118, 0.70710678118);
                    glUniform1f(m_dirLightLocation.DiffuseIntensity, 0.5);
                    /// For now, we are just using ambient + one directional light, no points or spots
                    glUniform1i(m_numPointLightsLocation, 0);
                    glUniform1i(m_numSpotLightsLocation, 0);

                    glBindVertexArray( robot_meshes[idx]->m_Entries[jj].VA );

                    robot_meshes[idx]->m_Textures[jj]->Bind(GL_TEXTURE0);

                    glDrawElements( GL_TRIANGLES, robot_meshes[idx]->m_Entries[jj].NumIndices, GL_UNSIGNED_INT, 0 );

                    glBindVertexArray( 0 );

                    glUseProgram( 0 );
                }else{

                    // ----- Render Model rendering -----
                    glUseProgram( m_unLitRGBModelProgramID );

                    Matrix4 matMVP = GetCurrentViewProjectionMatrix( nEye ) * GetRobotMatrixPose(robot_meshes[idx]->frame_id);
                    Matrix4 matWorld = GetRobotMatrixPose(robot_meshes[idx]->frame_id) ;
                    Vector4 eyePos = GetHMDMatrixPoseEye(nEye)*Vector4(0,0,0,1);
                    glUniformMatrix4fv( m_nLitRGBModelMatrixLocation, 1, GL_FALSE, matMVP.get() );

                    /// The settings for light number, location, intensity, colour, etc could
                    /// all be made ros params so that they could be user settable.
                    glUniformMatrix4fv( m_WorldMatrixRGBLocation, 1, GL_FALSE, matWorld.get() );
                    glUniform3f(m_eyeWorldPosRGBLocation, eyePos.x,eyePos.y,eyePos.z);
                    glUniform1i(m_colorTextureRGBLocation, 0);
                    glUniform3f(m_dirLightRGBLocation.Color, 1.0,1.0,1.0);
                    glUniform1f(m_dirLightRGBLocation.AmbientIntensity, 0.15);
                    glUniform3f(m_dirLightRGBLocation.Direction, 0.70710678118, 0, 0.70710678118);
                    glUniform1f(m_dirLightRGBLocation.DiffuseIntensity, 0.5);
                    /// For now, we are just using ambient + one directional light, no points or spots
                    glUniform1i(m_numPointLightsRGBLocation, 0);
                    glUniform1i(m_numSpotLightsRGBLocation, 0);

                    glBindVertexArray( robot_meshes[idx]->m_Entries[jj].VA );

                    glDrawElements( GL_TRIANGLES, robot_meshes[idx]->m_Entries[jj].NumIndices, GL_UNSIGNED_INT, 0 );

                    glBindVertexArray( 0 );

                    glUseProgram( 0 );
                }
            }
        }
    }

	glUseProgram( 0 );
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::RenderCompanionWindow()
{
	glDisable(GL_DEPTH_TEST);
	glViewport( 0, 0, m_nCompanionWindowWidth, m_nCompanionWindowHeight );

	glBindVertexArray( m_unCompanionWindowVAO );
	glUseProgram( m_unCompanionWindowProgramID );

	// render left eye (first half of index array )
	glBindTexture(GL_TEXTURE_2D, leftEyeDesc.m_nResolveTextureId );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glDrawElements( GL_TRIANGLES, m_uiCompanionWindowIndexSize/2, GL_UNSIGNED_SHORT, 0 );

	// render right eye (second half of index array )
	glBindTexture(GL_TEXTURE_2D, rightEyeDesc.m_nResolveTextureId  );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glDrawElements( GL_TRIANGLES, m_uiCompanionWindowIndexSize/2, GL_UNSIGNED_SHORT, (const void *)(uintptr_t)(m_uiCompanionWindowIndexSize) );

	glBindVertexArray( 0 );
	glUseProgram( 0 );
}


//-----------------------------------------------------------------------------
// Purpose: Gets a Matrix Projection Eye with respect to nEye.
//-----------------------------------------------------------------------------
Matrix4 CMainApplication::GetHMDMatrixProjectionEye( vr::Hmd_Eye nEye )
{
	if ( !m_pHMD )
		return Matrix4();

	vr::HmdMatrix44_t mat = m_pHMD->GetProjectionMatrix( nEye, m_fNearClip, m_fFarClip );

	return Matrix4(
		mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0],
		mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1], 
		mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2], 
		mat.m[0][3], mat.m[1][3], mat.m[2][3], mat.m[3][3]
	);
}


//-----------------------------------------------------------------------------
// Purpose: Gets an HMDMatrixPoseEye with respect to nEye.
//-----------------------------------------------------------------------------
Matrix4 CMainApplication::GetHMDMatrixPoseEye( vr::Hmd_Eye nEye )
{
	if ( !m_pHMD )
		return Matrix4();

	vr::HmdMatrix34_t matEyeRight = m_pHMD->GetEyeToHeadTransform( nEye );
	Matrix4 matrixObj(
		matEyeRight.m[0][0], matEyeRight.m[1][0], matEyeRight.m[2][0], 0.0, 
		matEyeRight.m[0][1], matEyeRight.m[1][1], matEyeRight.m[2][1], 0.0,
		matEyeRight.m[0][2], matEyeRight.m[1][2], matEyeRight.m[2][2], 0.0,
		matEyeRight.m[0][3], matEyeRight.m[1][3], matEyeRight.m[2][3], 1.0f
		);

	return matrixObj.invert();
}


//-----------------------------------------------------------------------------
// Purpose: Gets an HMDMatrixPoseEye with respect to nEye.
//-----------------------------------------------------------------------------
Matrix4 CMainApplication::GetRobotMatrixPose( std::string frame_name )
{
	return Matrix4().identity();
}


//-----------------------------------------------------------------------------
// Purpose: Gets a Current View Projection Matrix with respect to nEye,
//          which may be an Eye_Left or an Eye_Right.
//-----------------------------------------------------------------------------
Matrix4 CMainApplication::GetCurrentViewProjectionMatrix( vr::Hmd_Eye nEye )
{
	Matrix4 matMVP;
	if( nEye == vr::Eye_Left )
	{
		matMVP = m_mat4ProjectionLeft * m_mat4eyePosLeft * m_mat4HMDPose;
	}
	else if( nEye == vr::Eye_Right )
	{
		matMVP = m_mat4ProjectionRight * m_mat4eyePosRight *  m_mat4HMDPose;
	}

	return matMVP;
}


//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMainApplication::UpdateHMDMatrixPose()
{
	if ( !m_pHMD )
		return;

	vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0 );

	m_iValidPoseCount = 0;
	m_strPoseClasses = "";
	for ( int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice )
	{
		if ( m_rTrackedDevicePose[nDevice].bPoseIsValid )
		{
			m_iValidPoseCount++;
			m_rmat4DevicePose[nDevice] = ConvertSteamVRMatrixToMatrix4( m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking );
			if (m_rDevClassChar[nDevice]==0)
			{
				switch (m_pHMD->GetTrackedDeviceClass(nDevice))
				{
				case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
				case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
				case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
				case vr::TrackedDeviceClass_GenericTracker:    m_rDevClassChar[nDevice] = 'G'; break;
				case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
				default:                                       m_rDevClassChar[nDevice] = '?'; break;
				}
			}
			m_strPoseClasses += m_rDevClassChar[nDevice];
		}
	}

	if ( m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
	{
		m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
		m_mat4HMDPose.invert();
	}
}


//-----------------------------------------------------------------------------
// Purpose: Finds a render model we've already loaded or loads a new one
//-----------------------------------------------------------------------------
CGLRenderModel *CMainApplication::FindOrLoadRenderModel( const char *pchRenderModelName )
{
	CGLRenderModel *pRenderModel = NULL;
	for( std::vector< CGLRenderModel * >::iterator i = m_vecRenderModels.begin(); i != m_vecRenderModels.end(); i++ )
	{
		if( !stricmp( (*i)->GetName().c_str(), pchRenderModelName ) )
		{
			pRenderModel = *i;
			break;
		}
	}

	// load the model if we didn't find one
	if( !pRenderModel )
	{
		vr::RenderModel_t *pModel;
		vr::EVRRenderModelError error;
		while ( 1 )
		{
			error = vr::VRRenderModels()->LoadRenderModel_Async( pchRenderModelName, &pModel );
			if ( error != vr::VRRenderModelError_Loading )
				break;

			ThreadSleep( 1 );
		}

		if ( error != vr::VRRenderModelError_None )
		{
			dprintf( "Unable to load render model %s - %s\n", pchRenderModelName, vr::VRRenderModels()->GetRenderModelErrorNameFromEnum( error ) );
			return NULL; // move on to the next tracked device
		}

		vr::RenderModel_TextureMap_t *pTexture;
		while ( 1 )
		{
			error = vr::VRRenderModels()->LoadTexture_Async( pModel->diffuseTextureId, &pTexture );
			if ( error != vr::VRRenderModelError_Loading )
				break;

			ThreadSleep( 1 );
		}

		if ( error != vr::VRRenderModelError_None )
		{
			dprintf( "Unable to load render texture id:%d for render model %s\n", pModel->diffuseTextureId, pchRenderModelName );
			vr::VRRenderModels()->FreeRenderModel( pModel );
			return NULL; // move on to the next tracked device
		}

		pRenderModel = new CGLRenderModel( pchRenderModelName );
		if ( !pRenderModel->BInit( *pModel, *pTexture ) )
		{
			dprintf( "Unable to create GL model from render model %s\n", pchRenderModelName );
			delete pRenderModel;
			pRenderModel = NULL;
		}
		else
		{
			m_vecRenderModels.push_back( pRenderModel );
		}
		vr::VRRenderModels()->FreeRenderModel( pModel );
		vr::VRRenderModels()->FreeTexture( pTexture );
	}
	return pRenderModel;
}


//-----------------------------------------------------------------------------
// Purpose: Converts a SteamVR matrix to our local matrix class
//-----------------------------------------------------------------------------
Matrix4 CMainApplication::ConvertSteamVRMatrixToMatrix4( const vr::HmdMatrix34_t &matPose )
{
	Matrix4 matrixObj(
		matPose.m[0][0], matPose.m[1][0], matPose.m[2][0], 0.0,
		matPose.m[0][1], matPose.m[1][1], matPose.m[2][1], 0.0,
		matPose.m[0][2], matPose.m[1][2], matPose.m[2][2], 0.0,
		matPose.m[0][3], matPose.m[1][3], matPose.m[2][3], 1.0f
		);
	return matrixObj;
}


//-----------------------------------------------------------------------------
// Purpose: Create/destroy GL Render Models
//-----------------------------------------------------------------------------
CGLRenderModel::CGLRenderModel( const std::string & sRenderModelName )
	: m_sModelName( sRenderModelName )
{
	m_glIndexBuffer = 0;
	m_glVertArray = 0;
	m_glVertBuffer = 0;
	m_glTexture = 0;
}


CGLRenderModel::~CGLRenderModel()
{
	Cleanup();
}


//-----------------------------------------------------------------------------
// Purpose: Allocates and populates the GL resources for a render model
//-----------------------------------------------------------------------------
bool CGLRenderModel::BInit( const vr::RenderModel_t & vrModel, const vr::RenderModel_TextureMap_t & vrDiffuseTexture )
{
	// create and bind a VAO to hold state for this model
	glGenVertexArrays( 1, &m_glVertArray );
	glBindVertexArray( m_glVertArray );

	// Populate a vertex buffer
	glGenBuffers( 1, &m_glVertBuffer );
	glBindBuffer( GL_ARRAY_BUFFER, m_glVertBuffer );
	glBufferData( GL_ARRAY_BUFFER, sizeof( vr::RenderModel_Vertex_t ) * vrModel.unVertexCount, vrModel.rVertexData, GL_STATIC_DRAW );

	// Identify the components in the vertex buffer
	glEnableVertexAttribArray( 0 );
	glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( vr::RenderModel_Vertex_t ), (void *)offsetof( vr::RenderModel_Vertex_t, vPosition ) );
	glEnableVertexAttribArray( 1 );
	glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, sizeof( vr::RenderModel_Vertex_t ), (void *)offsetof( vr::RenderModel_Vertex_t, vNormal ) );
	glEnableVertexAttribArray( 2 );
	glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, sizeof( vr::RenderModel_Vertex_t ), (void *)offsetof( vr::RenderModel_Vertex_t, rfTextureCoord ) );

	// Create and populate the index buffer
	glGenBuffers( 1, &m_glIndexBuffer );
	glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, m_glIndexBuffer );
	glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( uint16_t ) * vrModel.unTriangleCount * 3, vrModel.rIndexData, GL_STATIC_DRAW );

	glBindVertexArray( 0 );

	// create and populate the texture
	glGenTextures(1, &m_glTexture );
	glBindTexture( GL_TEXTURE_2D, m_glTexture );

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, vrDiffuseTexture.unWidth, vrDiffuseTexture.unHeight,
		0, GL_RGBA, GL_UNSIGNED_BYTE, vrDiffuseTexture.rubTextureMapData );

	// If this renders black ask McJohn what's wrong.
	glGenerateMipmap(GL_TEXTURE_2D);

	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );

	GLfloat fLargest;
	glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest );

	glBindTexture( GL_TEXTURE_2D, 0 );

	m_unVertexCount = vrModel.unTriangleCount * 3;

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Frees the GL resources for a render model
//-----------------------------------------------------------------------------
void CGLRenderModel::Cleanup()
{
	if( m_glVertBuffer )
	{
		glDeleteBuffers(1, &m_glIndexBuffer);
		glDeleteVertexArrays( 1, &m_glVertArray );
		glDeleteBuffers(1, &m_glVertBuffer);
		m_glIndexBuffer = 0;
		m_glVertArray = 0;
		m_glVertBuffer = 0;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Draws the render model
//-----------------------------------------------------------------------------
void CGLRenderModel::Draw()
{
	glBindVertexArray( m_glVertArray );

	glActiveTexture( GL_TEXTURE0 );
	glBindTexture( GL_TEXTURE_2D, m_glTexture );

	glDrawElements( GL_TRIANGLES, m_unVertexCount, GL_UNSIGNED_SHORT, 0 );

	glBindVertexArray( 0 );
}

