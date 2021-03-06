/*
    Copyright (C) <2015>  <sniperHW@163.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _WPACKET_H
#define _WPACKET_H

#include <stdarg.h>
#include "comm.h"    
#include "packet/packet.h"
#include "util/endian.h"
#include "util/log.h"


typedef struct
{
    packet          base;
    buffer_writer   writer;
    TYPE_HEAD      *len;
}wpacket;


wpacket *wpacket_new(TYPE_HEAD size);


static inline void wpacket_data_copy(wpacket *w,bytebuffer *buf)
{
    TYPE_HEAD copy_size;
    char *ptr = buf->data;
    bytebuffer *from = cast(packet*,w)->head;
    TYPE_HEAD   pos  = cast(packet*,w)->spos; 
    TYPE_HEAD   size = cast(packet*,w)->len_packet; 
    do{
        copy_size = from->size - pos;
        if(copy_size > size) copy_size = size;
        memcpy(ptr,from->data+pos,copy_size);
        size -= copy_size;
        ptr  += copy_size;
        from  = from->next;
        pos   = 0;
    }while(size);
    buf->size = cast(packet*,w)->len_packet;
}

static inline void wpacket_copy_on_write(wpacket *w)
{
    bytebuffer *newbuff;
    uint32_t size = size_of_pow2(cast(packet*,w)->len_packet);
    if(size < MIN_BUFFER_SIZE) size = MIN_BUFFER_SIZE;
    newbuff = bytebuffer_new(size);
    wpacket_data_copy(w,newbuff);
    refobj_dec(cast(refobj*,cast(packet*,w)->head));
    cast(packet*,w)->head = newbuff;
    //set writer to the end
    buffer_writer_init(&w->writer,newbuff,cast(packet*,w)->len_packet);
    w->len = cast(TYPE_HEAD*,newbuff->data);
}


static inline void wpacket_expand(wpacket *w,TYPE_HEAD size)
{
    size = size_of_pow2(size);
    if(size < MIN_BUFFER_SIZE) size = MIN_BUFFER_SIZE;
    w->writer.cur->next = bytebuffer_new(size);
    buffer_writer_init(&w->writer,w->writer.cur->next,0);
}

static inline void wpacket_write(wpacket *w,char *in,TYPE_HEAD size)
{
    TYPE_HEAD ret;
    TYPE_HEAD packet_len = cast(packet*,w)->len_packet;
    TYPE_HEAD new_size   = packet_len + size;
    assert(new_size > packet_len);
    if(new_size < packet_len){
        //超过了包大小限制(64k)
        SYS_LOG(LOG_ERROR,"error on [%s:%d]:packet overflow\n",__FILE__,__LINE__);
        return;
    }
    if(!w->writer.cur)
        wpacket_copy_on_write(w);
    do{
        if(!w->writer.cur || 0 == (ret = cast(TYPE_HEAD,buffer_write(&w->writer,in,cast(uint32_t,size)))))
            wpacket_expand(w,size);
        else{
            in += ret;
            size -= ret;
        }
    }while(size);
    cast(packet*,w)->len_packet = new_size;
    *w->len = hton(new_size - SIZE_HEAD); 
}

static inline void wpacket_write_uint8(wpacket *w,uint8_t value)
{  
    wpacket_write(w,cast(char*,&value),sizeof(value));
}

static inline void wpacket_write_uint16(wpacket *w,uint16_t value)
{
    value = _hton16(value);
    wpacket_write(w,cast(char*,&value),sizeof(value));        
}

static inline void wpacket_write_uint32(wpacket *w,uint32_t value)
{   
    value = _hton32(value);
    wpacket_write(w,cast(char*,&value),sizeof(value));
}

static inline void wpacket_write_uint64(wpacket *w,uint64_t value)
{   
    value = _hton64(value);
    wpacket_write(w,cast(char*,&value),sizeof(value));
}

static inline void wpacket_write_double(wpacket *w,uint64_t value)
{   
    wpacket_write(w,cast(char*,&value),sizeof(value));
}

static inline void wpacket_write_binary(wpacket *w,const void *value,TYPE_HEAD size)
{
    assert(value);
#if TYPE_HEAD == uint16_t
    wpacket_write_uint16(w,size);
#else
    wpacket_write_uint32(w,size);
#endif    
    wpacket_write(w,cast(char*,value),size);
}

static inline void wpacket_write_string(wpacket *w ,const char *value)
{
    wpacket_write_binary(w,value,strlen(value)+1);
}


typedef struct wpacket_book{
    buffer_writer writer;
    void (*write)(struct wpacket_book *_book,...);
}wpacket_book;


static inline void _write_book_uint8(wpacket_book *_book,...)
{  
    va_list vl;
    va_start(vl,_book);
    uint8_t value = va_arg(vl,uint8_t);
    buffer_write(&_book->writer,cast(char*,&value),sizeof(value));
}

static inline void _write_book_uint16(wpacket_book *_book,...)
{
    va_list vl;
    va_start(vl,_book);
    uint16_t value = _hton16(va_arg(vl,uint16_t));
    buffer_write(&_book->writer,cast(char*,&value),sizeof(value));       
}

static inline void _write_book_uint32(wpacket_book *_book,...)
{   
    va_list vl;
    uint32_t value;
    va_start(vl,_book);
    value = _hton32(va_arg(vl,uint32_t));
    buffer_write(&_book->writer,cast(char*,&value),sizeof(value));
}

static inline void _write_book_uint64(wpacket_book *_book,...)
{   
    va_list vl;
    uint64_t value;
    va_start(vl,_book);
    value = _hton64(va_arg(vl,uint64_t));
    buffer_write(&_book->writer,cast(char*,&value),sizeof(value));
}

static inline void _write_book_double(wpacket_book *_book,...)
{   
    va_list vl;
    double  value;
    va_start(vl,_book);
    value = va_arg(vl,double);
    buffer_write(&_book->writer,cast(char*,&value),sizeof(value));
}


//book space and fill with 0

static inline wpacket_book wpacket_book_uint8(wpacket *w)
{  
    uint8_t value = 0;
    wpacket_book book = {.writer = w->writer,.write = _write_book_uint8};
    wpacket_write(w,cast(char*,&value),sizeof(value));
    return book;
}

static inline wpacket_book wpacket_book_uint16(wpacket *w)
{
    uint16_t value = 0;
    wpacket_book book = {.writer = w->writer,.write = _write_book_uint16};    
    wpacket_write(w,cast(char*,&value),sizeof(value));
    return book;        
}

static inline wpacket_book wpacket_book_uint32(wpacket *w)
{   
    uint32_t value = 0;
    wpacket_book book = {.writer = w->writer,.write = _write_book_uint32};
    wpacket_write(w,cast(char*,&value),sizeof(value));
    return book;
}

static inline wpacket_book wpacket_book_uint64(wpacket *w)
{   
    uint64_t value = 0;
    wpacket_book book = {.writer = w->writer,.write = _write_book_uint64};    
    wpacket_write(w,cast(char*,&value),sizeof(value));
    return book;
}

static inline wpacket_book wpacket_book_double(wpacket *w)
{   
    double value = 0;
    wpacket_book book = {.writer = w->writer,.write = _write_book_double};    
    wpacket_write(w,cast(char*,&value),sizeof(value));
    return book;
}




#endif  