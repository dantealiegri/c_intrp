#include "interpreter_element.h"

#include <stdlib.h>
#include <glibmm/ustring.h>
#include <glib/gprintf.h>
#include <libelf.h>
#include <gelf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

namespace Interpreter
{

class Libraries : public Element
{
	static Glib::ustring defaultLibraryPath[];
	public:
	Libraries( Element * parent = 0 ) : Element( parent )
	{
		g_printf("Interpreter::Libraries starting");
		if( elf_version(EV_CURRENT) == EV_NONE )
		{
			g_printf("Arg, Interpreter::Libraries barfed on init");
			return;
		}

		for( int i = 0; !defaultLibraryPath[i].empty() ;i++ )
		{
			  loadLibrary( defaultLibraryPath[i] );
		}


	}

	void addLine( Glib::ustring raw_input )
	{
		if( raw_input[0] == '#' ) 
		{
			generateError( "Interperter::Bulitins: doesn't handle preprocessor macros");
		}
	}

	protected:

	void loadLibrary( Glib::ustring path )
	{
		g_printf("I:L:loadLibrary: loading %s\n", path.c_str());
		int lfd = open( path.c_str(), O_RDONLY, 0 );
		int errsv = errno;
		if( lfd < 0 ) { g_printf("Failed to open library %s, %s\n", path.c_str(), strerror(errsv)); return; }

		Elf * eh = elf_begin( lfd, ELF_C_READ, NULL );

		if( ! eh ) { g_printf("Failed to read library %s", path.c_str()); return; }

		Elf_Kind ek = elf_kind(eh);

		g_printf("Elf is code: %i\n", ek );

		if( ek != ELF_K_ELF )
		{
			g_printf("Not an ELF object, cannot continue.\n");
			return;
		}

#if 0
		int elfclass;

		if( (elfclass = gelf_getclass(eh)) == ELFCLASSNONE )
		{
			g_printf("Library Endianness could not be determiend, aborting load\n");
			return;
		}

		g_printf("%s: %i-bit library\n", path.c_str(),  elfclass == ELFCLASS32 ? 32 : 64);


		Elf32_Ehdr * ehdr;

		if( ( ehdr =  elf32_getehdr( eh )) == NULL )
		{
			int errsv = errno;
			g_printf("Could not find any ELF segments in file: %s",elf_errmsg(-1));
			return;
		}
		int num_ph = ehdr->e_phnum;

		GElf_Phdr phdr;

		for( int i = 0; i < num_ph; i++ )
		{
			if( gelf_getphdr( eh, i, &phdr ) != &phdr )
			{
				g_printf("Could not retrieve ELF segment %i in file %s: %s",i,path.c_str(),elf_errmsg(-1));
			}

			g_printf("%s'%i: %s (0x%x) @ 0x%x\n",path.c_str(),i,ptype_to_string( phdr.p_type ), phdr.p_type, phdr.p_offset);
		}
#endif

		size_t shstrndx, shnum;

		Elf_Scn * escn = NULL;
		GElf_Shdr shdr;
		char * name;

		bool no_scount = false;

		if( elf_getshnum( eh, &shnum) == -1 )
		{
			g_printf("%s:Could not retrieve section header number.\n",path.c_str());
			return;
		}

		if( shnum == 0 ) // section count is in sh_size of section header 0
		{
			no_scount = true;
		}
		else
			g_printf("%s: has %i sections.\n",path.c_str(), shnum );

		int gsn_ret;
		if( ( gsn_ret = elf_getshstrndx( eh, &shstrndx)) == -1 )
		{
			g_printf("%s:Could not retrieve section header index.\n",path.c_str());
			return;
		}
		g_printf("Section header index is:%li\n",shstrndx);

		if( gsn_ret == SHN_XINDEX )
		{
			g_printf("File uses extended index\n");
		}

		GElf_Shdr *nh = (GElf_Shdr*)malloc(sizeof( GElf_Shdr));

		while(( escn = elf_nextscn( eh, escn )) != NULL )
		{
			if( gelf_getshdr( escn,nh) != nh )
			{
				g_printf("%s:Could not retrieve section header.\n",path.c_str());
				return;
			}

			if(( name = elf_strptr( eh, shstrndx, nh->sh_name )) == NULL )
			{
				g_printf("%s:Could not find name for section header.\n",path.c_str());
				return;
			}

			Elf32_Shdr * eshdr = elf32_getshdr( escn );
			Elf_Data * d = NULL;
			while(  ( d = (elf_getdata( escn , d ))) != NULL )
			{
				const char * type_name =  "[[invalid]]";
				if( d != NULL )
					type_name = stype_to_string( d->d_type );

				g_printf("%s'%i: %s - %s\n",path.c_str(),elf_ndxscn(escn), name,  type_name);
				if( d && d->d_type == ELF_T_GNUHASH ) 
					parse_gnu_hash( d );
				else if( d && d->d_type == ELF_T_SYM ) 
					parse_symbol_table( name,  d, eh, eshdr );
			}
		}

		elf_end( eh );
		close( lfd );
	}

	void parse_symbol_table( const char * st_name , Elf_Data * ed, Elf * e, Elf32_Shdr * shdr )
	{
#define BIND_TYPE( S , T ) (ELF32_ST_BIND( S->st_info) == T )
		Elf32_Sym * sym = (Elf32_Sym*)ed->d_buf;
		Elf32_Sym * lastsym = (Elf32_Sym*)( (char*)ed->d_buf + ed->d_size);
		g_printf( "================= % 0s SYMBOL TABLE =================\n",st_name);
		int num = 0;
		const char * name;
		for( ; sym < lastsym; sym++, num++ )
		{
			if( (sym->st_value == 0 ) ||
			    (BIND_TYPE( sym, STB_WEAK )) ||
			    (BIND_TYPE( sym, STT_FUNC )) ||
			    (BIND_TYPE( sym, STB_NUM )) )
				continue;
			name = elf_strptr(e, shdr->sh_link, (size_t) sym->st_name );
			g_printf("%05i:  % 48s\n",num,name);
		}
		g_printf("=======================================================\n");
	}

	void parse_gnu_hash( Elf_Data * es )
	{
		uint32_t * array = (uint32_t*)es->d_buf;
		uint32_t nbuckets = array[0];
		uint32_t symndx = array[1];
		uint32_t maskwords = array[2];
		uint32_t shift2 = array[3];

		if( maskwords % 2 != 0 )
		{
			g_printf("I:L:parse_gnu_hash: maskwords malformed ( not power of 2 )\n");
			return;
		}

		uint32_t * bloom_filter = array+4;
		uint32_t * hash_buckets = array + 4 + maskwords;
		uint32_t * hash_values = array + 4 + nbuckets + maskwords;

		g_printf( "======== GNU_HASH ========\n"
		          " hash buckets:    % 8i\n"
			  " symbol index:    % 8i\n"
			  " bloom filter size: % 6i\n"
			  " bloom filter shift: % 5i\n"
			  " bloom filter: @0x%08x\n"
			  " bucket:       @0x%08x\n"
			  " values:       @0x%08x\n"
			  "==========================\n",
			   nbuckets,symndx,maskwords,shift2,
			   bloom_filter, hash_buckets,hash_values);
	}


#define CZ(T) case PT_##T: ret = #T; break;
	const char * ptype_to_string( size_t type )
	{
		const char * ret;
		switch( type )
		{
			CZ(NULL); CZ(LOAD); CZ(DYNAMIC); CZ(INTERP); CZ(NOTE);
			CZ(SHLIB); CZ(PHDR); CZ(TLS); CZ(SUNWBSS);
			CZ(SUNWSTACK); CZ(NUM); CZ(GNU_EH_FRAME);
			CZ(GNU_STACK); CZ(GNU_RELRO);
			default:
				ret = "unknown";
				break;
		}
		return ret;
	}
#define SZ(T) case ELF_T_##T: ret = #T; break;
	const char * stype_to_string( Elf_Type type )
	{
		const char * ret;
		switch( type )
		{
			SZ(BYTE); SZ(ADDR); SZ(DYN); SZ(EHDR); SZ(HALF); SZ(OFF); SZ( PHDR);
			SZ(RELA); SZ(REL); SZ(SHDR); SZ(SWORD); SZ(SYM); SZ(WORD); SZ(XWORD);
			SZ(SXWORD); SZ(VDEF); SZ(VDAUX); SZ(VNEED); SZ(VNAUX); SZ(NHDR);
			SZ(SYMINFO); SZ(MOVE); SZ(LIB);SZ(GNUHASH); SZ(AUXV); SZ(NUM);
			default:
				ret = "unknown";
				break;
		}
		return ret;
	}

};
Glib::ustring Libraries::defaultLibraryPath[] = 
{
//	 Glib::ustring("/lib/libc.so.6" ),
	 Glib::ustring("/usr/lib/libglib-2.0.so" ),
	  Glib::ustring()  };
}
