//***********************************************************************************************
//     The contents of this file are subject to the Mozilla Public License
//     Version 1.1 (the "License"); you may not use this file except in
//     compliance with the License. You may obtain a copy of the License at
//     http://www.mozilla.org/MPL/
//
//     Software distributed under the License is distributed on an "AS IS"
//     basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
//     License for the specific language governing rights and limitations
//     under the License.
//
//     The Original Code is ReynardWare Incredimail Converter.
//
//     The Initial Developer of the Original Code is David P. Owczarski, created March 20, 2009
//
//     Contributor(s):
//
//************************************************************************************************

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <Shellapi.h>
#include <winuser.h>
#include <shlobj.h>
#include <richedit.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "resource.h"
#include "Increadimail_Convert.h"

BOOL CALLBACK Winload( HWND, UINT, WPARAM, LPARAM );
//***************************************************************************
// INPUTS:
//     HWND hdwnd - The handle of the Dialog Box Window
//     UNIT message - The dialog box's window messages
//     WPARM wparam - Wparam for the dialog box 
//     LPAMRAM lparam - Lparam for the dialog box 
//
// OUTPUTS:  None
//
// RETURN VALUE: BOOL - (1) Function success and (0) Failure
//
// DESCRIPTION:
//   This is the main function of the Dialog box.
//
//***************************************************************************

BOOL CALLBACK About_Box( HWND, UINT, WPARAM, LPARAM );
//***************************************************************************
// INPUTS:
//
// OUTPUTS:
//
// RETURN VALUE:
//
// DESCRIPTION:
//
//***************************************************************************

void WINAPI process_emails();
//***************************************************************************
// INPUTS: None
//
// OUTPUTS: None
//
// RETURN VALUE: Void
//
// DESCRIPTION:  This function processes all the emails within a dialog box
//
//***************************************************************************

typedef enum {
   THREAD_NOT_STARTED = 0,
   THREAD_IN_PROGRESS = 1,
   THREAD_COMPLETED = 2,
} thread_status ;

HINSTANCE hThisInst;         // This instance
HWND global_hwnd;            // global window handle for the progress
thread_status email_thread;  // thread status

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int sShowCmd ) {
MSG msg;

   hThisInst = hInstance;

   // Load the Fox icon
   LoadIcon( hInstance, MAKEINTRESOURCE( IDI_REYNARD ) );

   // Start the DialogBox
   DialogBox( hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC) Winload );

   while(GetMessage( &msg, NULL, 0, 0 ) ) {
      TranslateMessage( &msg );
      DispatchMessage( &msg );
   }

   return (int) msg.wParam;
}


BOOL CALLBACK Winload( HWND hdwnd, UINT message, WPARAM wparam, LPARAM lparam ) {

// strings
char file_title[MAX_CHAR];
char window_title[MAX_CHAR];
char filter[MAX_CHAR];
char im_database_filename[MAX_CHAR];
char im_header_filename[MAX_CHAR];
char im_attachments_directory[MAX_CHAR];
char debug_str[MAX_CHAR];
char debug_str2[MAX_CHAR];
char version[5];

int e_count;  // email count
int d_count;  // deleted email count
static int blink = 0;  // blink status

OPENFILENAME         openfile;
BROWSEINFO           bi;
LPITEMIDLIST         idlist;
ITEMIDLIST           idlistspace;
INITCOMMONCONTROLSEX cc;
static HANDLE        hThread;
DWORD                dwThreadId;
DWORD                ExitCode;

   // Always Initialize Memory
   ZeroMemory( &file_title, sizeof( file_title ) );
   ZeroMemory( &im_database_filename, sizeof( im_database_filename ) );
   ZeroMemory( &im_header_filename, sizeof( im_header_filename ) );
   ZeroMemory( &im_attachments_directory, sizeof( im_attachments_directory ) );

   // check on the status of the email thread
   if( email_thread == THREAD_COMPLETED ) {
      KillTimer(hdwnd, 1);

      // reset the status of the thread
      email_thread = THREAD_NOT_STARTED;
   }

   switch( message ) {
      case WM_INITDIALOG:          
         // Setup progress bar
         ZeroMemory( &cc, sizeof( cc ) );
         cc.dwSize = sizeof( INITCOMMONCONTROLSEX );
         cc.dwICC  = ICC_PROGRESS_CLASS;
         SendDlgItemMessage( hdwnd, IDC_PROGRESS1, PBM_SETSTEP, (WPARAM) 1, 0 );

         //Default for exporting all the emails
         //SendDlgItemMessage( hdwnd, IDC_CHECK1, BM_SETCHECK, (WPARAM) BST_CHECKED, 0);
         
         // set this up for a second thread
         global_hwnd = hdwnd;  
      return 1;

      case WM_TIMER:
         switch (wparam) {
         case 1:
            if( email_thread == THREAD_IN_PROGRESS ) {
               ZeroMemory( &debug_str2, sizeof( debug_str2 ) );
               blink = !blink;
               if( blink ) {
                  sprintf_s( debug_str2, MAX_CHAR, "*Converting*" );
                  SetDlgItemText( hdwnd, IDC_STATIC9, debug_str2 );
               } else {
                  sprintf_s( debug_str2, MAX_CHAR, " " );
                  SetDlgItemText( hdwnd, IDC_STATIC9, debug_str2 );            
               }
            } else {
               sprintf_s( debug_str2, MAX_CHAR, " " );
               SetDlgItemText( hdwnd, IDC_STATIC9, debug_str2 );
            }
         }
         return 1; 
      return 1;

      case WM_COMMAND:
         switch(LOWORD(wparam)) {

            case IDC_BROWSE:  // The database browse button
               ZeroMemory( &window_title, sizeof( window_title ) );
               ZeroMemory( &openfile, sizeof( openfile ) );
               ZeroMemory( &filter, sizeof( filter ) );
               ZeroMemory( &version, sizeof( version ) );

               openfile.hwndOwner      = hdwnd;
               openfile.lStructSize    = sizeof( openfile );
               openfile.Flags          = OFN_READONLY;
               openfile.nMaxFile       = sizeof( im_database_filename );
               openfile.lpstrFileTitle = file_title;
               openfile.lpstrFile      = &im_database_filename[0];
               openfile.nMaxFileTitle  = sizeof( file_title );

               strcpy_s( window_title, sizeof("Open Incredimail Database File"), "Open Incredimail Database File");
               openfile.lpstrTitle = window_title;

               strcpy_s( filter, sizeof("Incredimail Database *.imm\0*.imm\0\0"), "Incredimail Database *.imm\0*.imm\0\0" );
               openfile.lpstrFilter = &filter[0];

               strcpy_s( im_database_filename, sizeof("*.imm"), "*.imm" );

               if( GetOpenFileName( &openfile ) == TRUE ) {
                  SetDlgItemText( hdwnd, IDC_EDIT1, im_database_filename );
                  strncpy_s( im_header_filename, MAX_CHAR ,im_database_filename, strlen( im_database_filename ) - 3 );
                  strcat_s( im_header_filename, MAX_CHAR, "imh" );
                  email_count( im_header_filename, &e_count, &d_count );

                  // get version of the database -- report it is v4, v5, or unknown
                  ZeroMemory( &version, sizeof( version ) );
                  get_database_version( im_header_filename, version );
                  if( strcmp( version, "V#04" ) == 0 ) {
                     SetDlgItemText( hdwnd, IDC_STATIC6, "Version: 4" );
                  } else if( strcmp( version, "V#05" ) == 0 ) {
                     SetDlgItemText( hdwnd, IDC_STATIC6, "Version: 5" );
                  } else {
                     SetDlgItemText( hdwnd, IDC_STATIC6, "Version: n/a" );
                     MessageBox( hdwnd, "Unknown format!\nConverting will produce unexpected results!", "Warning", MB_OK );
                  }

                  sprintf_s( debug_str, MAX_CHAR, "Email Count: %d", e_count );
                  SetDlgItemText( hdwnd, IDC_ECOUNT, debug_str );
                  sprintf_s( debug_str, MAX_CHAR, "Deleted Emails: %d", d_count );
                  SetDlgItemText( hdwnd, IDC_STATIC8, debug_str );
               }
               return 1;

            case IDC_BROWSE2:  // the attachement browse button
               ZeroMemory( &bi, sizeof( bi ) );
               ZeroMemory( &idlist, sizeof( idlist ) );
               strcpy_s( file_title, MAX_CHAR, "Select the attachment directory" );

               bi.pszDisplayName = im_attachments_directory;
               bi.lpszTitle = file_title;
               bi.pidlRoot = idlist;

               idlist = &idlistspace;
               idlist = SHBrowseForFolder( &bi );
               SHGetPathFromIDList( idlist, im_attachments_directory );
               SetDlgItemText( hdwnd, IDC_EDIT2, im_attachments_directory );
               return 1;

            case IDOK:  // OK Button
               if( email_thread == THREAD_NOT_STARTED || email_thread == THREAD_COMPLETED ) {
                  GetDlgItemText( hdwnd, IDC_EDIT1, (LPSTR) &im_database_filename, 256 );       // get the incredimail database name
                  GetDlgItemText( hdwnd, IDC_EDIT2, (LPSTR) &im_attachments_directory, 256 );   // get the attachement directory name

                  if( im_database_filename[0] != '\0' && im_attachments_directory[0] != '\0' ) {
                     // execute a thread for processing emails
                     hThread = CreateThread( NULL, 0, (LPTHREAD_START_ROUTINE) process_emails, 0, 0, &dwThreadId );
                     if( hThread ) {
                        email_thread = THREAD_IN_PROGRESS;
                     }
                     SetTimer( hdwnd, 1, 500, (TIMERPROC) NULL );
                     SendDlgItemMessage( hdwnd, IDOK, BN_DISABLE, 0, 0 );
                  } else {
                     // display an error if the database or attachment directory is NULL
                     if( im_database_filename[0] == '\0' ) {
                        sprintf_s( debug_str, sizeof("Need Incredimail database filename"), "Need Incredimail database filename" );
                        MessageBox( hdwnd, debug_str, "Error!", MB_OK );
                     } else {
                        sprintf_s( debug_str, sizeof("Need attachment directory"), "Need attachment directory" );
                        MessageBox( hdwnd, debug_str, "Error!", MB_OK );
                     }
                  }
               }
            return 1;

            case IDCANCEL:  // Cancel Button

               // End thread if in progress
               if( email_thread == THREAD_IN_PROGRESS ) {
                  GetExitCodeThread( hThread, &ExitCode );
                  TerminateThread( hThread, ExitCode );
                  DeleteFile( "attachment.bin" );
               }
               CloseHandle( hThread );

               // End Dialog
               EndDialog( hdwnd, 0 );
               PostQuitMessage( 0 );
            return 1;

            case IDC_ABOUT:  // About button
                DialogBox( hThisInst, MAKEINTRESOURCE(IDD_ABOUT), NULL, (DLGPROC) About_Box );
            return 1;
         }
   }

   return 0;
}

// TODO: FIX THIS!
BOOL CALLBACK About_Box( HWND hdwnd, UINT message, WPARAM wparam, LPARAM lparam ) {
LoadLibrary(TEXT("Riched20.dll"));


   switch( message ) {
      case WM_INITDIALOG:
         SetDlgItemText( hdwnd, IDC_RICHEDIT26, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033\\deflangfe1033{\\fonttbl{\\f0\\fswiss\\fprq2\\fcharset0 Arial;}}{\\colortbl ;\\red0\\green0\\blue255;}\\viewkind4\\uc1\\pard\\f0\\fs20{\\field{\\*\\fldinst{HYPERLINK ""mailto:ReynardWare@gmail.com""}}{\\fldrslt{\\cf1\\ul ReynardWare@gmail.com}}}\\cf0\\ulnone\\f0\\fs20\\par}" );
         SendDlgItemMessage( hdwnd, IDC_RICHEDIT26, EM_SETEVENTMASK, ENM_LINK, (LPARAM) NULL );
         SendDlgItemMessage( hdwnd, IDC_RICHEDIT26, EM_AUTOURLDETECT, FALSE, (LPARAM) NULL );
         return 1;
 
      case WM_COMMAND:
         switch(LOWORD(wparam)) {
            case IDOK:
               EndDialog( hdwnd, 0 );
            return 1;
         }
   }

   return 0;
}


void WINAPI process_emails() {

char im_header_filename[MAX_CHAR];
char im_database_filename[MAX_CHAR];
char im_attachments_directory[MAX_CHAR];
char debug_str[MAX_CHAR];
char eml_filename[MAX_CHAR];
char new_eml_filename[MAX_CHAR];
char export_directory[MAX_CHAR];
char temp_directory[MAX_CHAR];

int e_count, d_count;
int deleted_email, export_all_email;
unsigned int offset, size;
int i, result_header, result_database, result_attachment;
struct _stat buf;
float percent_complete;
int real_count = 0;

   // Zero out the string names
   ZeroMemory( &im_header_filename, sizeof( im_header_filename ) );
   ZeroMemory( &im_database_filename, sizeof( im_database_filename ) );
   ZeroMemory( &im_attachments_directory, sizeof( im_attachments_directory ) );
   ZeroMemory( &new_eml_filename, sizeof( new_eml_filename ) );
   ZeroMemory( &export_directory, sizeof( export_directory ) );
   ZeroMemory( &temp_directory, sizeof( temp_directory ) );

   SendDlgItemMessage( global_hwnd, IDC_PROGRESS1, PBM_SETPOS, 0, 0 );                 // reset the progress bar to 0%

   GetDlgItemText( global_hwnd, IDC_EDIT1, (LPSTR) &im_database_filename, 256 );       // get the incredimail database name
   GetDlgItemText( global_hwnd, IDC_EDIT2, (LPSTR) &im_attachments_directory, 256 );   // get the attachement directory name

   // get the header filename
   strncpy_s( im_header_filename, MAX_CHAR, im_database_filename, strlen( im_database_filename ) - 3 );
   strcat_s( im_header_filename, MAX_CHAR, "imh" );

   result_header     = _stat( im_header_filename, &buf );
   result_database   = _stat( im_database_filename, &buf );
   result_attachment = _stat( im_attachments_directory, &buf );

   // check if file or directory exists
   if( result_header != 0 || result_database != 0 || result_attachment != 0  ) {
      if( result_header != 0 ) {
         MessageBox( global_hwnd, "Can't open associated .imh file", "Error!", MB_OK );
      } else if( result_database != 0 ) {
         MessageBox( global_hwnd, "Can't open the Incredimail database file", "Error!", MB_OK );
      } else if( result_attachment != 0 ) {
         MessageBox( global_hwnd, "Can't open attachment directory", "Error!", MB_OK );
      }
   } else {
      // the export directory is based off of the database name
      strncpy_s( export_directory, MAX_CHAR, im_header_filename, strlen( im_database_filename ) - 4 );
      DeleteDirectory( export_directory );
      CreateDirectory( export_directory, NULL );
      strcat_s( export_directory, MAX_CHAR, "\\" );

      // set the email and deleted count to zero
      e_count = 0;
      d_count = 0;

      email_count( im_header_filename, &e_count, &d_count );

      // get the state of the checkbox
      export_all_email = (int) SendDlgItemMessage( global_hwnd, IDC_CHECK1, BM_GETCHECK, 0, 0);

      SendDlgItemMessage( global_hwnd, IDC_PROGRESS1, PBM_SETRANGE, 0, (LPARAM) MAKELPARAM (0, e_count));

      // create a temp directory
      strncpy_s( temp_directory, MAX_CHAR, im_header_filename, strlen( im_database_filename ) - 4 );
      strcat_s( temp_directory, MAX_CHAR, "_temp" );
      DeleteDirectory( temp_directory );
      CreateDirectory( temp_directory, NULL ); 
      strcat_s( temp_directory, MAX_CHAR, "\\" );

      for( i = 0; i < e_count; i++ ) {
         offset = 0;
         get_email_offset_and_size( im_header_filename, &offset, &size, i, e_count, &deleted_email );

         if( (export_all_email == BST_CHECKED) || !deleted_email ) {
            // setup the temp eml file name
            ZeroMemory( &eml_filename, sizeof( eml_filename ) );
            strcpy_s( eml_filename, MAX_CHAR, temp_directory  );
            sprintf_s( new_eml_filename, MAX_CHAR, "email%d.eml", i );
            strcat_s( eml_filename, MAX_CHAR, new_eml_filename );

            // extract the eml file in the temp directory
            extract_eml_files( im_database_filename, eml_filename, offset, size );

            ZeroMemory( export_directory, sizeof( export_directory ) );
            strncpy_s( export_directory, MAX_CHAR, im_header_filename, strlen( im_database_filename ) - 4 );
            strcat_s( export_directory, MAX_CHAR, "\\" );
            strcat_s( export_directory, MAX_CHAR, new_eml_filename );
            insert_attachments( eml_filename, im_attachments_directory, export_directory );

         }
         // update the progress
         percent_complete =  ( ( (float) (i+1)/ (float) e_count) ) * 100;
         sprintf_s( debug_str, MAX_CHAR, "%d of %d (%0.0f%%)", i+1 ,e_count, percent_complete );
         SetDlgItemText( global_hwnd, IDC_XOFX, debug_str );
         SendDlgItemMessage( global_hwnd, IDC_PROGRESS1, PBM_STEPIT, 0, 0 );
         SendMessage( global_hwnd, WM_PAINT, 0, 0 );
      }
   
      // clean up, state it was done and delete the temp directory
      sprintf_s( debug_str, MAX_CHAR, "%d of %d DONE!",i ,e_count );
      SetDlgItemText( global_hwnd, IDC_XOFX, debug_str );

      // remove the temporary directory
      temp_directory[strlen(temp_directory)-1]='\0';  // remove the slash or it will not work
      DeleteDirectory( temp_directory );
      email_thread = THREAD_COMPLETED;
   }
}
