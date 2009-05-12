#ifndef _INTERPRETER_DEBUG_CHANNELS_H_
#define _INTERPRETER_DEBUG_CHANNELS_H_

#include <glibmm.h>
#include <stdarg.h>

#include "util.h"

namespace Interpreter
{
	class DebugChannel
	{
		public:
		DebugChannel( const gchar * channel_name, DebugChannel * _parent = 0 )
		{
			parent = _parent;
			explicitly_enabled = false;

			name = channel_name;
		}
		~DebugChannel()
		{
		}

		void setEnabled( bool v )
		{
			printf("%s::setEnabled(%s)\n",name.c_str(),(v?"true":"false"));
			explicitly_enabled = v;
		}

		void xprint( const gchar * format, ...  )
		{
			va_list L;
			va_start( L,format );
			print( format , L );
			va_end( L );
		}
		void print( const gchar * format , ... ) // va_list  args )
		{
			va_list L;
			va_start( L,format );
			if( parent && ! explicitly_enabled ) parent->print_owned( name.c_str(), format, L );
			else if( explicitly_enabled )
			{
				g_vprintf( format, L );
			}
			va_end( L );
		}

		protected:
		bool explicitly_enabled;
		Glib::ustring name;
		DebugChannel * parent;

		void print_owned( const gchar * owned ,  const gchar * format , ... ) // va_list  args )
		{
			va_list L;
			va_start( L,format );
			if( parent && ! explicitly_enabled ) parent->print_owned( owned, format, L );
			else if( explicitly_enabled )
			{
				gchar fnew[1024];
				snprintf( fnew, 1024, "%s:%s", name.c_str(), format );
				g_vprintf( fnew , L );
			}
			va_end( L );
		}
	};

	class Debug
	{
		static Debug * _globalDebugInstance;
		public:
		Debug()
		{
			channel_hierarchy = g_hash_table_new( g_str_hash, g_str_equal );
		}
		~Debug()
		{
		}

		// can be a single name like "library_load"
		// or "library_load.library", the second shows the hierarchy,
		// this means that you can disable "library" and it will disable library_load
		// or you can enable library, and it will enable all subchannels,
		// though enabling library_load will not enable all library.
		//
		// other reserved characters are : , # ; and '
		// reasoning: , is to split channels. ( x , y , x )
		// reasoning: # is used, below.
		// reasoining: ' : ; are reserved for future use.
		//
		// to implement, shornames: #input lineinput.root
		//  this means "make a link name input, which refers to lineinput.root
		//

		static DebugChannel * addChannel( Glib::ustring name )
		{
			initIfMissing();
			return _globalDebugInstance->_globalAddDebugChannel( name );
		}

		static void setEnabledChannels( const char * list )
		{
			initIfMissing();
			_globalDebugInstance->_globalSetEnabledChannels( list );
		}

		static void print( Glib::ustring channel, Glib::ustring message )
		{
			initIfMissing();
		}

		protected:
		// List channels

		GHashTable * channel_hierarchy;

		typedef struct _internal_pair
		{
			DebugChannel * c;
			GHashTable * t;
		} InternalPair;
			
		void _globalSetEnabledChannels( Glib::ustring list )
		{
			// TODO: split list.
			GSList * in_order_l = split_top( list );
			GSList * l =  g_slist_reverse( in_order_l );
			DebugChannel * to_enable = _getChannel( l );
			if( to_enable)
			{
				to_enable->setEnabled( true );
			}
		}

		DebugChannel * _getChannel( GSList * id )
		{
			DebugChannel * r = NULL;
			GHashTable * curTable = channel_hierarchy;
			int pos = 0;
			while( pos <g_slist_length( id ))
			{
				Glib::ustring * c = (Glib::ustring*) g_slist_nth_data( id, pos );
				const gchar * key = c->c_str();
				gpointer v;
				if( ( v = g_hash_table_lookup( curTable, key ) ) != NULL )
				{
					InternalPair *p = (InternalPair*)v;
					if( pos + 1 < g_slist_length( id ))
					{
						if( p->t == NULL )
						{
							g_print("Debug::_getChannel( %s ): %s is not a channel group.\n", print_slist( id ) , key ); 
							break;
						}
						curTable = p->t;
					}
					else // end
						r = p->c;
				}
				else
				{
					g_print("Debug::_getChannel( %s ): %s doesn't exist.\n",
					print_slist( id ) , key ); 

					break;
				}
				pos++;
			}
			return r;
		}

		const gchar * print_slist( GSList * id )
		{
			int pos = 0;
			GSList * tmp = NULL;
			while( pos <g_slist_length( id ))
			{
				Glib::ustring * c = (Glib::ustring*) g_slist_nth_data( id, pos );
				const gchar * name = c->c_str();
				tmp = g_slist_append( tmp , strdup(name));

				if( pos + 1 < g_slist_length( id ))
					tmp = g_slist_append( tmp , strdup("." ));

				pos++;
			}

			const char * ret = sprint_gchar_slist( tmp );
			// TODO: delete tmp and data.
			//
			return ret;

		}

		DebugChannel * _globalAddDebugChannel( Glib::ustring name )
		{
			DebugChannel * final = NULL;
			DebugChannel * lastChannel = NULL;
			GSList * in_order_l = split_top( name );
			GSList * l =  g_slist_reverse( in_order_l );
			GHashTable * curTable = channel_hierarchy;
			printf("_globalAddDebugChannel(%s)\n",name.c_str());

			while( g_slist_length( l ) != 0 )
			{
				Glib::ustring * c = (Glib::ustring*) g_slist_nth_data( l, 0 );
				const gchar * key = c->c_str();
				gpointer v;
				// if key exists
				if( ( v = g_hash_table_lookup( curTable, key ) ) != NULL )
				{
					InternalPair *p = (InternalPair*)v;
					// last item, and key is a group
					if( g_slist_length( l ) == 1 && p->t != NULL )
					{
						final = p->c;
						g_printf("Redefinition of already existing channel group: %s\n", c->c_str());
					}
					else if( g_slist_length( l ) == 1 )
					{
						final = p->c;
						g_printf("Redefinition of already existing channel: %s\n", c->c_str());
					}
					else // just traversing the tree.
					{
						if( p->t == NULL ) // change channel to group
						{
							p->t  = g_hash_table_new( g_str_hash, g_str_equal );
							g_printf("*Created new channel, %s\n",key);
						}
						lastChannel = p->c;
						curTable = p->t;
					}
					
				}
				else // doesn't exist.
				{
					gchar * new_key = strdup( key );
					InternalPair * new_pair = (InternalPair*)malloc( sizeof(InternalPair));
					memset( new_pair,0,sizeof( InternalPair));
					new_pair->t = NULL;

					new_pair->c = new DebugChannel( key,lastChannel );

					g_hash_table_insert( curTable, new_key, new_pair );
					if( g_slist_length( l ) == 1 )
					{
						g_printf("*Created new channel, %s\n",key);
						final = new_pair->c;
					}
					else
					{
						g_printf("*Created new group, %s\n",key);
						GHashTable *nh = g_hash_table_new( g_str_hash, g_str_equal );
						new_pair->t = nh;
						curTable = new_pair->t;
						lastChannel = new_pair->c;

					}
				}
//				g_printf("List size: %i\n",g_slist_length( l ));
				l = g_slist_remove( l, c );
				delete c;
				//g_printf("List size: %i\n",g_slist_length( l ));
			}
			return final;
		}

		static void initIfMissing()
		{
			if( _globalDebugInstance == NULL )
			{
				_globalDebugInstance = new Debug();
			}
		}

		inline GSList * split_top( Glib::ustring in )
		{
			GSList * s = NULL; //g_slist_alloc();
//			printf("Begin: size %i\n",g_slist_length( s ));
			Glib::ustring::size_type p = 0;
			int c = 0;
			Glib::ustring::size_type l = -1;
//			printf("Searching in [%s]\n",in.c_str());
			for(  ;(l = in.find( ".", p )) > 0 && c < 10; c++ )
			{
//				printf("%i:%i,%i\n",c,l,p);
				Glib::ustring *small = new Glib::ustring(in.substr( p,l-p));
//				printf("%i:%i,%i [%s]\n",c,l,p, small->c_str());
				s = g_slist_append( s , small );
				if( l != Glib::ustring::npos ) p = l+1;
				else
					break;
			}
//			printf("End: size %i\n",g_slist_length( s ));
			return s;
		}

		
	};

	Debug *Debug::_globalDebugInstance = NULL;
}
#endif // _INTERPRETER_DEBUG_CHANNELS_H_
