
#include <sys/wait.h>

#include "../sys_public.h"

void	OutputDebugString( const char* text );

// input
void 	Sys_InitInput();
void 	Sys_ShutdownInput();

void	IN_DeactivateMouse();
void	IN_ActivateMouse();

void	IN_Activate( bool active );
void	IN_Frame();

void* wglGetProcAddress( const char* name );

void	Sleep( const int time );

void	Sys_UpdateWindowMouseInputRect();
