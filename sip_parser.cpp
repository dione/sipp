/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Author : Richard GAYRAUD - 04 Nov 2003
 *           Olivier Jacques
 *           From Hewlett Packard Company.
 *           Shriram Natarajan
 *           Peter Higginson
 *           Eric Miller
 *           Venkatesh
 *           Enrico Hartung
 *           Nasir Khan
 *           Lee Ballard
 *           Guillaume Teissier from FTR&D
 *           Wolfgang Beck
 *           Venkatesh
 *           Vlad Troyanker
 *           Charles P Wright from IBM Research
 *           Amit On from Followap
 *           Jan Andres from Freenet
 *           Ben Evans from Open Cloud
 *           Marc Van Diest from Belgacom
 *	     Stefan Esser
 *           Andy Aicken
 */

#include <string.h>
#include <stdlib.h>
#include "screen.hpp"
#include "sip_parser.hpp"

/*************************** Mini SIP parser ***************************/

char * get_peer_tag(char *msg)
{
    char        * to_hdr;
    char        * ptr;
    char        * end_ptr;
    static char   tag[MAX_HEADER_LEN];
    int           tag_i = 0;

    to_hdr = strstr(msg, "\r\nTo:");
    if(!to_hdr) to_hdr = strstr(msg, "\r\nto:");
    if(!to_hdr) to_hdr = strstr(msg, "\r\nTO:");
    if(!to_hdr) to_hdr = strstr(msg, "\r\nt:");
    if(!to_hdr) {
        ERROR("No valid To: header in reply");
    }

    // Remove CRLF
    to_hdr += 2;

    end_ptr = strchr(to_hdr,'\n');

    ptr = strchr(to_hdr, '>');
    if (!ptr) {
        return NULL;
    }

    ptr = strchr(to_hdr, ';');

    if(!ptr) {
        return NULL;
    }

    to_hdr = ptr;

    ptr = strstr(to_hdr, "tag");
    if(!ptr) {
        ptr = strstr(to_hdr, "TAG");
    }
    if(!ptr) {
        ptr = strstr(to_hdr, "Tag");
    }

    if(!ptr) {
        return NULL;
    }

    if (ptr>end_ptr) {
        return NULL ;
    }

    ptr = strchr(ptr, '=');

    if(!ptr) {
        ERROR("Invalid tag param in To: header");
    }

    ptr ++;

    while((*ptr != ' ')  &&
            (*ptr != ';')  &&
            (*ptr != '\t') &&
            (*ptr != '\r') &&
            (*ptr != '\n') &&
            (*ptr)) {
        tag[tag_i++] = *(ptr++);
    }
    tag[tag_i] = 0;

    return tag;
}

char * get_incoming_header_content(char* message, const char * name)
{
    /* non reentrant. consider accepting char buffer as param */
    static char last_header[MAX_HEADER_LEN * 10];
    char * src, *dest, *ptr;

    /* returns empty string in case of error */
    memset(last_header, 0, sizeof(last_header));

    if((!message) || (!strlen(message))) {
        return last_header;
    }

    src = message;
    dest = last_header;

    /* for safety's sake */
    if (NULL == name || NULL == strrchr(name, ':')) {
        return last_header;
    }

    while((src = strstr(src, name))) {

        /* just want the header's content */
        src += strlen(name);

        ptr = strchr(src, '\n');

        /* Multiline headers always begin with a tab or a space
         * on the subsequent lines */
        while((ptr) &&
                ((*(ptr+1) == ' ' ) ||
                 (*(ptr+1) == '\t')    )) {
            ptr = strchr(ptr + 1, '\n');
        }

        if(ptr) {
            *ptr = 0;
        }
        // Add "," when several headers are present
        if (dest != last_header) {
            dest += sprintf(dest, ",");
        }
        dest += sprintf(dest, "%s", src);
        if(ptr) {
            *ptr = '\n';
        }

        src++;
    }

    if(dest == last_header) {
        return last_header;
    }

    *(dest--) = 0;

    /* Remove trailing whitespaces, tabs, and CRs */
    while ((dest > last_header) &&
            ((*dest == ' ') || (*dest == '\r')|| (*dest == '\t'))) {
        *(dest--) = 0;
    }

    /* remove enclosed CRs in multilines */
    while((ptr = strchr(last_header, '\r'))) {
        /* Use strlen(ptr) to include trailing zero */
        memmove(ptr, ptr+1, strlen(ptr));
    }

    return last_header;
}

char * get_incoming_first_line(char * message)
{
    /* non reentrant. consider accepting char buffer as param */
    static char last_header[MAX_HEADER_LEN * 10];
    char * src;

    /* returns empty string in case of error */
    memset(last_header, 0, sizeof(last_header));

    if((!message) || (!strlen(message))) {
        return last_header;
    }

    src = message;

    int i=0;
    while (*src) {
        if((*src=='\n')||(*src=='\r')) {
            break;
        } else {
            last_header[i]=*src;
        }
        i++;
        src++;
    }

    return last_header;
}


char * get_call_id(char *msg)
{
    static char call_id[MAX_HEADER_LEN];
    char * ptr1, * ptr2, * ptr3, backup;
    bool short_form;

    call_id[0] = '\0';

    short_form = false;

    ptr1 = strstr(msg, "Call-ID:");
    if(!ptr1) {
        ptr1 = strstr(msg, "Call-Id:");
    }
    if(!ptr1) {
        ptr1 = strstr(msg, "Call-id:");
    }
    if(!ptr1) {
        ptr1 = strstr(msg, "call-Id:");
    }
    if(!ptr1) {
        ptr1 = strstr(msg, "call-id:");
    }
    if(!ptr1) {
        ptr1 = strstr(msg, "CALL-ID:");
    }
    // For short form, we need to make sure we start from beginning of line
    // For others, no need to
    if(!ptr1) {
        ptr1 = strstr(msg, "\r\ni:");
        short_form = true;
    }
    if(!ptr1) {
        WARNING("(1) No valid Call-ID: header in reply '%s'", msg);
        return call_id;
    }

    if (short_form) {
        ptr1 += 4;
    } else {
        ptr1 += 8;
    }

    while((*ptr1 == ' ') || (*ptr1 == '\t')) {
        ptr1++;
    }

    if(!(*ptr1)) {
        WARNING("(2) No valid Call-ID: header in reply");
        return call_id;
    }

    ptr2 = ptr1;

    while((*ptr2) &&
            (*ptr2 != ' ') &&
            (*ptr2 != '\t') &&
            (*ptr2 != '\r') &&
            (*ptr2 != '\n')) {
        ptr2 ++;
    }

    if(!*ptr2) {
        WARNING("(3) No valid Call-ID: header in reply");
        return call_id;
    }

    backup = *ptr2;
    *ptr2 = 0;
    if ((ptr3 = strstr(ptr1, "///")) != 0) ptr1 = ptr3+3;
    strcpy(call_id, ptr1);
    *ptr2 = backup;
    return (char *) call_id;
}

unsigned long int get_cseq_value(char *msg)
{
    char *ptr1;


    // no short form for CSeq:
    ptr1 = strstr(msg, "\r\nCSeq:");
    if(!ptr1) {
        ptr1 = strstr(msg, "\r\nCSEQ:");
    }
    if(!ptr1) {
        ptr1 = strstr(msg, "\r\ncseq:");
    }
    if(!ptr1) {
        ptr1 = strstr(msg, "\r\nCseq:");
    }
    if(!ptr1) {
        WARNING("No valid Cseq header in request %s", msg);
        return 0;
    }

    ptr1 += 7;

    while((*ptr1 == ' ') || (*ptr1 == '\t')) {
        ++ptr1;
    }

    if(!(*ptr1)) {
        WARNING("No valid Cseq data in header");
        return 0;
    }

    return strtoul(ptr1, NULL, 10);
}

unsigned long get_reply_code(char *msg)
{
    while((msg) && (*msg != ' ') && (*msg != '\t')) msg ++;
    while((msg) && ((*msg == ' ') || (*msg == '\t'))) msg ++;

    if ((msg) && (strlen(msg)>0)) {
        return atol(msg);
    } else {
        return 0;
    }
}

