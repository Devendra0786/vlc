/*****************************************************************************
 * m3u.c: a meta demux to parse pls, m3u and asx playlists
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: m3u.c,v 1.19 2003/06/26 14:42:04 zorglub Exp $
 *
 * Authors: Sigmund Augdal <sigmunau@idi.ntnu.no>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>                                              /* strdup() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_playlist.h>

#include <sys/types.h>

/*****************************************************************************
 * Constants and structures
 *****************************************************************************/
#define MAX_LINE 1024

#define TYPE_UNKNOWN 0
#define TYPE_M3U 1
#define TYPE_ASX 2
#define TYPE_HTML 3
#define TYPE_PLS 4
#define TYPE_B4S 5

struct demux_sys_t
{
    int i_type;                                   /* playlist type (m3u/asx) */
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate  ( vlc_object_t * );
static void Deactivate( vlc_object_t * );
static int  Demux ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("playlist metademux") );
    set_capability( "demux", 180 );
    set_callbacks( Activate, Deactivate );
    add_shortcut( "m3u" );
    add_shortcut( "asx" );
    add_shortcut( "html" );
    add_shortcut( "pls" );
    add_shortcut( "b4s" );
vlc_module_end();

/*****************************************************************************
 * Activate: initializes m3u demux structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    char           *psz_ext;
    demux_sys_t    *p_m3u;
    int             i_type = 0;
    int             i_type2 = 0;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    p_input->pf_demux = Demux;
    p_input->pf_rewind = NULL;

    /* Check for m3u/asx file extension or if the demux has been forced */
    psz_ext = strrchr ( p_input->psz_name, '.' );

    if( ( psz_ext && !strcasecmp( psz_ext, ".m3u") ) ||
        ( p_input->psz_demux && !strcmp(p_input->psz_demux, "m3u") ) )
    {
        i_type = TYPE_M3U;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".asx") ) ||
             ( p_input->psz_demux && !strcmp(p_input->psz_demux, "asx") ) )
    {
        i_type = TYPE_ASX;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".html") ) ||
             ( p_input->psz_demux && !strcmp(p_input->psz_demux, "html") ) )
    {
        i_type = TYPE_HTML;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".pls") ) ||
             ( p_input->psz_demux && !strcmp(p_input->psz_demux, "pls") ) )
    {
        i_type = TYPE_PLS;
    }
    else if( ( psz_ext && !strcasecmp( psz_ext, ".b4s") ) ||
             ( p_input->psz_demux && !strcmp(p_input->psz_demux, "b4s") ) )
    {
        i_type = TYPE_B4S;
    }

    /* we had no luck looking at the file extention, so we have a look
     * at the content. This is useful for .asp, .php and similar files
     * that are actually html. Also useful for som asx files that have
     * another extention */
    if( i_type != TYPE_M3U )
    {
        byte_t *p_peek;
        int i_size = input_Peek( p_input, &p_peek, MAX_LINE );
        i_size -= sizeof("[playlist]") - 1;
        if ( i_size > 0 ) {
            while ( i_size
                    && strncasecmp( p_peek, "[playlist]", sizeof("[playlist]") - 1 )
                    && strncasecmp( p_peek, "<html>", sizeof("<html>") - 1 )
                    && strncasecmp( p_peek, "<asx", sizeof("<asx") - 1 ) 
                    && strncasecmp( p_peek, "<?xml", sizeof("<?xml") -1 ) )
            {
                p_peek++;
                i_size--;
            }
            if ( !i_size )
            {
                ;
            }
            else if ( !strncasecmp( p_peek, "[playlist]", sizeof("[playlist]") -1 ) )
            {
                i_type2 = TYPE_PLS;
            }
            else if ( !strncasecmp( p_peek, "<html>", sizeof("<html>") -1 ) )
            {
                i_type2 = TYPE_HTML;
            }
            else if ( !strncasecmp( p_peek, "<asx", sizeof("<asx") -1 ) )
            {
                i_type2 = TYPE_ASX;
            }
            else if ( !strncasecmp( p_peek, "<?xml", sizeof("<?xml") -1 ) )
            {
                i_type2 = TYPE_B4S;
            }
            
        }
    }
    if ( !i_type && !i_type2 )
    {
        return -1;
    }
    if ( i_type  && !i_type2 )
    {
        i_type = TYPE_M3U;
    }
    else
    {
        i_type = i_type2;
    }

    /* Allocate p_m3u */
    if( !( p_m3u = malloc( sizeof( demux_sys_t ) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return -1;
    }
    p_input->p_demux_data = p_m3u;

    p_m3u->i_type = i_type;

    return 0;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
static void Deactivate( vlc_object_t *p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t *p_m3u = (demux_sys_t *)p_input->p_demux_data  ; 

    free( p_m3u );
}

/*****************************************************************************
 * XMLSpecialChars: Handle the special chars in a XML file. 
 * Returns 0 if successful
 * ***************************************************************************/
static int XMLSpecialChars ( char *psz_src , char *psz_dst )
{
    unsigned int i;
    unsigned int j=0;
    char c_rplc=0;    
    
    for( i=0 ; i < strlen(psz_src) ; i++ )
    {
        if( psz_src[i]  == '&')
        {
            if( !strncasecmp( &psz_src[i], "&#xe0;", 6) ) c_rplc = '�';
            else if( !strncasecmp( &psz_src[i], "&#xe9;", 6) ) c_rplc = '�';
            else if( !strncasecmp( &psz_src[i], "&#xee;", 6) ) c_rplc = '�';
            else if( !strncasecmp( &psz_src[i], "&apos;", 6) ) c_rplc = '\'';
            else if( !strncasecmp( &psz_src[i], "&#xe8;", 6) ) c_rplc = '�';
            else if( !strncasecmp( &psz_src[i], "&#xea;", 6) ) c_rplc = '�';
            else if( !strncasecmp( &psz_src[i], "&#xea;", 6) ) c_rplc = '�';
            psz_dst[j]=c_rplc;
            j++;
            i = i+6;
        }
        psz_dst[j] = psz_src[i];
        j++;
    }        
    psz_dst[j]='\0';
    return 0;
}


/*****************************************************************************
 * ProcessLine: read a "line" from the file and add any entries found
 * to the playlist. Return number of items added ( 0 or 1 )
 *****************************************************************************/
static int ProcessLine ( input_thread_t *p_input , demux_sys_t *p_m3u
        , playlist_t *p_playlist , char psz_line[MAX_LINE], int i_position )
{
    char          *psz_bol, *psz_name;
    
    psz_bol = psz_line;

    /* Remove unnecessary tabs or spaces at the beginning of line */
    while( *psz_bol == ' ' || *psz_bol == '\t' ||
           *psz_bol == '\n' || *psz_bol == '\r' )
        psz_bol++;

    if( p_m3u->i_type == TYPE_M3U )
    {
        /* Check for comment line */
        if( *psz_bol == '#' )
            /*line is comment or extended info, ignored for now */
            return 0;
    }
    else if ( p_m3u->i_type == TYPE_PLS )
    {
        /* We are dealing with .pls files from shoutcast
         * We are looking for lines like "File1=http://..." */
        if( !strncasecmp( psz_bol, "File", sizeof("File") - 1 ) )
        {
            psz_bol += sizeof("File") - 1;
            psz_bol = strchr( psz_bol, '=' );
            if ( !psz_bol ) return 0;
            psz_bol++;
        }
        else
        {
            return 0;
        }
    }
    else if ( p_m3u->i_type == TYPE_ASX )
    {
        /* We are dealing with ASX files.
         * We are looking for "<ref href=" xml markups that
         * begins with "mms://", "http://" or "file://" */
        char *psz_eol;

        while( *psz_bol &&
               strncasecmp( psz_bol, "ref", sizeof("ref") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        while( *psz_bol &&
               strncasecmp( psz_bol, "href", sizeof("href") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        while( *psz_bol &&
               strncasecmp( psz_bol, "mms://",
                            sizeof("mms://") - 1 ) &&
               strncasecmp( psz_bol, "mmsu://",
                            sizeof("mmsu://") - 1 ) &&
               strncasecmp( psz_bol, "mmst://",
                            sizeof("mmst://") - 1 ) &&
               strncasecmp( psz_bol, "http://",
                            sizeof("http://") - 1 ) &&
               strncasecmp( psz_bol, "file://",
                            sizeof("file://") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        psz_eol = strchr( psz_bol, '"');
        if( !psz_eol )
          return 0;

        *psz_eol = '\0';
    }
    else if ( p_m3u->i_type == TYPE_HTML )
    {
        /* We are dealing with a html file with embedded
         * video.  We are looking for "<param name="filename"
         * value=" html markups that begin with "http://" */
        char *psz_eol;

        while( *psz_bol &&
               strncasecmp( psz_bol, "param", sizeof("param") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        while( *psz_bol &&
               strncasecmp( psz_bol, "filename", sizeof("filename") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        while( *psz_bol &&
               strncasecmp( psz_bol, "http://",
                            sizeof("http://") - 1 ) )
            psz_bol++;

        if( !*psz_bol ) return 0;

        psz_eol = strchr( psz_bol, '"');
        if( !psz_eol )
          return 0;

        *psz_eol = '\0';

    }
    else if ( p_m3u->i_type == TYPE_B4S )
    {
        /* We are dealing with a B4S file from Winamp3
         * We are looking for <entry Playstring="blabla"> */

        char *psz_eol;
            
         while ( *psz_bol &&
         strncasecmp( psz_bol,"Playstring",sizeof("Playstring") -1 ) )
               psz_bol++;

       if( !*psz_bol ) return 0;

        psz_bol = strchr( psz_bol, '=' );
        if ( !psz_bol ) return 0;
              psz_bol++;
              psz_bol++;

        psz_eol= strchr(psz_bol, '"');
        if( !psz_eol ) return 0;

        *psz_eol= '\0';

        /* Handle the XML special characters */    
        if( XMLSpecialChars( psz_bol , psz_bol ) )
                return 0;
    }
    else
    {
        msg_Warn( p_input, "unknown file type" );
        return 0;
    }

    /* empty line */
    if ( !*psz_bol ) return 0;

    /*
     * From now on, we know we've got a meaningful line
     */

    /* check for a protocol name */
    /* for URL, we should look for "://"
     * for MRL (Media Resource Locator) ([[<access>][/<demux>]:][<source>]),
     * we should look for ":"
     * so we end up looking simply for ":"*/
    /* PB: on some file systems, ':' are valid characters though*/
    psz_name = psz_bol;
    while( *psz_name && *psz_name!=':' )
    {
        psz_name++;
    }
#ifdef WIN32
    if ( *psz_name && ( psz_name == psz_bol + 1 ) )
    {
        /* if it is not an URL,
         * as it is unlikely to be an MRL (PB: if it is ?)
         * it should be an absolute file name with the drive letter */
        if ( *(psz_name+1) == '/' )/* "*:/" */
        {
            if ( *(psz_name+2) != '/' )/* not "*://" */
                while ( *psz_name ) *psz_name++;/* so now (*psz_name==0) */
        }
        else while ( *psz_name ) *psz_name++;/* "*:*"*/
    }
#endif

    /* if the line doesn't specify a protocol name,
     * check if the line has an absolute or relative path */
#ifndef WIN32
    if( !*psz_name && *psz_bol != '/' )
         /* If this line doesn't begin with a '/' */
#else
    if( !*psz_name
            && *psz_bol!='/'
            && *psz_bol!='\\'
            && *(psz_bol+1)!=':' )
         /* if this line doesn't begin with
          *  "/" or "\" or "*:" or "*:\" or "*:/" or "\\" */
#endif
    {
        /* assume the path is relative to the path of the m3u file. */
        char *psz_path = strdup( p_input->psz_name );

#ifndef WIN32
        psz_name = strrchr( psz_path, '/' );
#else
        psz_name = strrchr( psz_path, '\\' );
        if ( ! psz_name ) psz_name = strrchr( psz_path, '/' );
#endif
        if( psz_name ) *psz_name = '\0';
        else *psz_path = '\0';
#ifndef WIN32
        psz_name = malloc( strlen(psz_path) + strlen(psz_bol) + 2 );
        sprintf( psz_name, "%s/%s", psz_path, psz_bol );
#else
        if ( *psz_path != '\0' )
        {
            psz_name = malloc( strlen(psz_path) + strlen(psz_bol) + 2 );
            sprintf( psz_name, "%s\\%s", psz_path, psz_bol );
        }
        else psz_name = strdup( psz_bol );
#endif
        free( psz_path );
    }
    else
    {
        psz_name = strdup( psz_bol );
    }

    playlist_Add( p_playlist, psz_name,
                  PLAYLIST_INSERT, i_position );

    free( psz_name );
    return 1;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux ( input_thread_t *p_input )
{
    data_packet_t *p_data;
    char          *p_buf, psz_line[MAX_LINE], eol_tok;
    int           i_size, i_bufpos, i_linepos = 0;
    playlist_t    *p_playlist;
    int           i_position;
    vlc_bool_t    b_discard = VLC_FALSE;

    demux_sys_t   *p_m3u = (demux_sys_t *)p_input->p_demux_data;

    p_playlist = (playlist_t *) vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                                 FIND_ANYWHERE );
    if( !p_playlist )
    {
        msg_Err( p_input, "can't find playlist" );
        return -1;
    }

    p_playlist->pp_items[p_playlist->i_index]->b_autodeletion = VLC_TRUE;
    i_position = p_playlist->i_index + 1;

    /* Depending on wether we are dealing with an m3u/asf file, the end of
     * line token will be different */
    if( p_m3u->i_type == TYPE_ASX || p_m3u->i_type == TYPE_HTML 
                    || p_m3u->i_type == TYPE_B4S )
        eol_tok = '>';
    else
        eol_tok = '\n';

    while( ( i_size = input_SplitBuffer( p_input, &p_data, MAX_LINE ) ) > 0 )
    {
        i_bufpos = 0; p_buf = p_data->p_payload_start;

        while( i_size )
        {
            /* Build a line < MAX_LINE */
            while( p_buf[i_bufpos] != eol_tok && i_size )
            {
                if( i_linepos == MAX_LINE || b_discard == VLC_TRUE )
                {
                    /* line is bigger than MAX_LINE, discard it */
                    i_linepos = 0;
                    b_discard = VLC_TRUE;
                }
                else
                {
                    if ( eol_tok != '\n' || p_buf[i_bufpos] != '\r' )
                    {
                        psz_line[i_linepos] = p_buf[i_bufpos];
                        i_linepos++;
                    }
                }

                i_size--; i_bufpos++;
            }

            /* Check if we need more data */
            if( !i_size ) continue;

            i_size--; i_bufpos++;
            b_discard = VLC_FALSE;

            /* Check for empty line */
            if( !i_linepos ) continue;

            psz_line[i_linepos] = '\0';
            i_linepos = 0;
            
            i_position += ProcessLine ( p_input, p_m3u , p_playlist ,
                                        psz_line, i_position );

        }

        input_DeletePacket( p_input->p_method_data, p_data );
    }

    if ( i_linepos && b_discard != VLC_TRUE && eol_tok == '\n' )
    {
        psz_line[i_linepos] = '\0';
        i_linepos = 0;
        i_position += ProcessLine ( p_input, p_m3u , p_playlist , psz_line,
                                    i_position );
    }

    vlc_object_release( p_playlist );

    return 0;
}
