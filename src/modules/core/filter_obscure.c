/*
 * filter_obscure.c -- obscure filter
 * Copyright (C) 2003-2004 Ushodaya Enterprises Limited
 * Author: Charles Yates <charles.yates@pandora.be>
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "filter_obscure.h"

#include <framework/mlt_frame.h>
#include <framework/mlt_deque.h>

#include <stdio.h>
#include <stdlib.h>

/** Geometry struct.
*/

struct geometry_s
{
	int nw;
	int nh;
	float x;
	float y;
	float w;
	float h;
	int mask_w;
	int mask_h;
};

/** Parse a value from a geometry string.
*/

static float parse_value( char **ptr, int normalisation, char delim, float defaults )
{
	float value = defaults;

	if ( *ptr != NULL && **ptr != '\0' )
	{
		char *end = NULL;
		value = strtod( *ptr, &end );
		if ( end != NULL )
		{
			if ( *end == '%' )
				value = ( value / 100.0 ) * normalisation;
			while ( *end == delim || *end == '%' )
				end ++;
		}
		*ptr = end;
	}

	return value;
}

/** Parse a geometry property string.
*/

static void geometry_parse( struct geometry_s *geometry, struct geometry_s *defaults, char *property, int nw, int nh )
{
	// Assign normalised width and height
	geometry->nw = nw;
	geometry->nh = nh;

	// Assign from defaults if available
	if ( defaults != NULL )
	{
		geometry->x = defaults->x;
		geometry->y = defaults->y;
		geometry->w = defaults->w;
		geometry->h = defaults->h;
		geometry->mask_w = defaults->mask_w;
		geometry->mask_h = defaults->mask_h;
	}
	else
	{
		geometry->mask_w = 20;
		geometry->mask_h = 20;
	}

	// Parse the geomtry string
	if ( property != NULL )
	{
		char *ptr = property;
		geometry->x = parse_value( &ptr, nw, ',', geometry->x );
		geometry->y = parse_value( &ptr, nh, ':', geometry->y );
		geometry->w = parse_value( &ptr, nw, 'x', geometry->w );
		geometry->h = parse_value( &ptr, nh, ':', geometry->h );
		geometry->mask_w = parse_value( &ptr, nw, 'x', geometry->mask_w );
		geometry->mask_h = parse_value( &ptr, nh, ' ', geometry->mask_h );
	}
}

/** A Timism but not as clean ;-).
*/

static float lerp( float value, float lower, float upper )
{
	if ( value < lower )
		return lower;
	else if ( value > upper )
		return upper;
	return value;
}

/** Calculate real geometry.
*/

static void geometry_calculate( struct geometry_s *output, struct geometry_s *in, struct geometry_s *out, float position, int ow, int oh )
{
	// Calculate this frames geometry
	output->x = lerp( ( in->x + ( out->x - in->x ) * position ) / ( float )out->nw * ow, 0, ow );
	output->y = lerp( ( in->y + ( out->y - in->y ) * position ) / ( float )out->nh * oh, 0, oh );
	output->w = lerp( ( in->w + ( out->w - in->w ) * position ) / ( float )out->nw * ow, 0, ow - output->x );
	output->h = lerp( ( in->h + ( out->h - in->h ) * position ) / ( float )out->nh * oh, 0, oh - output->y );
	output->mask_w = in->mask_w + ( out->mask_w - in->mask_w ) * position;
	output->mask_h = in->mask_h + ( out->mask_h - in->mask_h ) * position;
}

/** Calculate the position for this frame.
*/

static float position_calculate( mlt_filter this, mlt_frame frame )
{
	// Get the in and out position
	mlt_position in = mlt_filter_get_in( this );
	mlt_position out = mlt_filter_get_out( this );

	// Get the position of the frame
	mlt_position position = mlt_frame_get_position( frame );

	// Now do the calcs
	return ( float )( position - in ) / ( float )( out - in + 1 );
}

/** The averaging function...
*/

void obscure_average( uint8_t *start, int width, int height, int stride )
{
	int y;
	int x;
	register int Y = ( *start + *( start + 2 ) ) / 2;
	register int U = *( start + 1 );
	register int V = *( start + 3 );
	register uint8_t *p;

	for ( y = 0; y < height; y ++ )
	{
		p = start + y * stride;
		for ( x = 0; x < width / 2; x ++ )
		{
			Y = ( Y + *p ++ ) / 2;
			U = ( U + *p ++ ) / 2;
			Y = ( Y + *p ++ ) / 2;
			V = ( V + *p ++ ) / 2;
		}
	}

	for ( y = 0; y < height; y ++ )
	{
		p = start + y * stride;
		
		for ( x = 0; x < width / 2; x ++ )
		{
			*p ++ = Y;
			*p ++ = U;
			*p ++ = Y;
			*p ++ = V;
		}
	}
}


/** The obscurer rendering function...
*/

static void obscure_render( uint8_t *image, int width, int height, struct geometry_s result )
{
	int area_x = result.x;
	int area_y = result.y;
	int area_w = result.w;
	int area_h = result.h;

	int mw = result.mask_w;
	int mh = result.mask_h;
	int w;
	int h;
	int aw;
	int ah;

	uint8_t *p = image + area_y * width * 2 + area_x * 2;

	for ( w = 0; w < area_w; w += mw )
	{
		for ( h = 0; h < area_h; h += mh )
		{
			aw = w + mw > area_w ? mw - ( w + mw - area_w ) : mw;
			ah = h + mh > area_h ? mh - ( h + mh - area_h ) : mh;
			if ( aw > 1 && ah > 1 )
				obscure_average( p + h * width * 2 + w * 2, aw, ah, width * 2 );
		}
	}
}

/** Do it :-).
*/

static int filter_get_image( mlt_frame frame, uint8_t **image, mlt_image_format *format, int *width, int *height, int writable )
{
	// Get the frame properties
	mlt_properties frame_properties = mlt_frame_properties( frame );

	// Fetch the obscure stack for this frame
	mlt_deque deque = mlt_properties_get_data( frame_properties, "filter_obscure", NULL );

	// Pop the top of stack now
	mlt_filter this = mlt_deque_pop_back( deque );

	// Get the image from the frame
	int error = mlt_frame_get_image( frame, image, format, width, height, 1 );

	// Get the image from the frame
	if ( error == 0 && *format == mlt_image_yuv422 )
	{
		if ( this != NULL )
		{
			// Get the filter properties
			mlt_properties properties = mlt_filter_properties( this );

			// Obtain the normalised width and height from the frame
			int normalised_width = mlt_properties_get_int( frame_properties, "normalised_width" );
			int normalised_height = mlt_properties_get_int( frame_properties, "normalised_height" );

			// Structures for geometry
			struct geometry_s result;
			struct geometry_s start;
			struct geometry_s end;

			// Calculate the position
			float position = position_calculate( this, frame );

			// Now parse the geometries
			geometry_parse( &start, NULL, mlt_properties_get( properties, "start" ), normalised_width, normalised_height );
			geometry_parse( &end, &start, mlt_properties_get( properties, "end" ), normalised_width, normalised_height );

			// Do the calculation
			geometry_calculate( &result, &start, &end, position, *width, *height );

			// Now actually render it
			obscure_render( *image, *width, *height, result );
		}
	}

	return error;
}

/** Filter processing.
*/

static mlt_frame filter_process( mlt_filter this, mlt_frame frame )
{
	// Get the frame properties
	mlt_properties frame_properties = mlt_frame_properties( frame );

	// Fetch the obscure stack for this frame
	mlt_deque deque = mlt_properties_get_data( frame_properties, "filter_obscure", NULL );

	// Create stack if necessary
	if ( deque == NULL )
	{
		// Create the deque
		deque = mlt_deque_init( );

		// Assign to the frame
		mlt_properties_set_data( frame_properties, "filter_obscure", deque, 0, ( mlt_destructor )mlt_deque_close, NULL );
	}

	// Push this on to the obscure stack
	mlt_deque_push_back( deque, this );
	
	// Push the get image call
	mlt_frame_push_get_image( frame, filter_get_image );

	return frame;
}

/** Constructor for the filter.
*/

mlt_filter filter_obscure_init( void *arg )
{
	mlt_filter this = mlt_filter_new( );
	if ( this != NULL )
	{
		mlt_properties properties = mlt_filter_properties( this );
		this->process = filter_process;
		mlt_properties_set( properties, "start", arg != NULL ? arg : "0%,0%:100%x100%" );
		mlt_properties_set( properties, "end", "" );
	}
	return this;
}

