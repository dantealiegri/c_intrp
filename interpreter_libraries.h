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
#include <time.h>

#include "debug_channels.h"
#include "parsed_library.h"

namespace Interpreter
{

class Libraries : public Element
{
	static Glib::ustring defaultLibraryPath[];
	DebugChannel * init, *slisting, *symtab, *gnuhash, *unk_slisting, *othr_sects;
	DebugChannel * parse_error, *parse_overview, *dynsec, *vneed_parse;
	public:
	Libraries( Element * parent = 0 ) : Element( parent )
	{
		 init =  Debug::addChannel("init.library");
		 slisting  = Debug::addChannel("symbols.parsing.library");
		 unk_slisting  = Debug::addChannel("unknown_symbols.parsing_errors.library");
		 parse_error  = Debug::addChannel("generic_error.parsing_errors.library");
		 symtab  = Debug::addChannel("symboltable.parsing.library");
		 gnuhash  = Debug::addChannel("gnuhash.parsing.library");
		 othr_sects  = Debug::addChannel("other_sections.parsing.library");
		 dynsec  = Debug::addChannel("dyn.parsing.library");
		 parse_overview  = Debug::addChannel("overview.parsing.library");
		 vneed_parse  = Debug::addChannel("verneed.parsing.library");

		 init->setEnabled( true );
		 parse_error->setEnabled( true );
		 parse_overview->setEnabled( true );
		 dynsec->setEnabled( true );
		 othr_sects->setEnabled( false );
		 unk_slisting->setEnabled( true );
		 slisting->setEnabled( true );
		 vneed_parse->setEnabled( false );

		 vneed_parse->setPrefix("VNP: ");
		 unk_slisting->setPrefix("!!! ");
		 parse_overview->setPrefix("** ");
		 slisting->setPrefix("-+ ");
		 dynsec->setPrefix("PP ParseDyn.");


		 symtab->setEnabled( false );
		 gnuhash->setEnabled( false );
		init->print("Interpreter::Libraries starting");
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

	ParsedLibrary * workingLibrary;
	GSList * parse_waiting_stack;

	struct pws_item
	{
		Elf_Scn * this_scn;
		size_t waiting_on;
	};

	void loadLibrary( Glib::ustring path )
	{
		parse_waiting_stack  = NULL;
		workingLibrary = new ParsedLibrary( path );
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

		while( g_slist_length( parse_waiting_stack) != 0 || ( escn = elf_nextscn( eh, escn )) != NULL )
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
				const char * sh_type_name =  "[[invalid]]";
				if( d != NULL )
					type_name = stype_to_string( d->d_type );
				if( eshdr != NULL )
					sh_type_name = shtype_to_string( eshdr->sh_type );

				slisting->print("%s'%i: %s - %s - %s\n",path.c_str(),elf_ndxscn(escn), name, sh_type_name,  type_name);
				if( !d )
					continue;
				if( eshdr->sh_type == SHT_GNU_HASH ) 
					parse_gnu_hash( d );
				else if( eshdr->sh_type == SHT_SYMTAB ) 
					parse_symbol_table( name,  d, eh, escn );
				else if( eshdr->sh_type == SHT_NOTE  ) 
					parse_note( name,  d, eh, eshdr );
				else if( eshdr->sh_type == SHT_STRTAB ) 
					parse_string_table( name,  d, eh, eshdr );
				else if( eshdr->sh_type == SHT_PROGBITS ) 
					parse_progbits( name,  d, eh, eshdr );
				else if( eshdr->sh_type == SHT_GNU_versym ) 
					parse_gnu_version( name,  d, eh, eshdr );
				else if( eshdr->sh_type == SHT_GNU_verneed ) 
					parse_gnu_verneed( name,  d, eh, eshdr );
				else if( eshdr->sh_type == SHT_NOBITS ) 
					parse_bss( name,  d, eh, eshdr );
				else if( eshdr->sh_type == SHT_DYNAMIC ) 
					parse_dyn( name,  d, eh, eshdr );
				else if( eshdr->sh_type == SHT_REL ) 
					parse_rel( name,  d, eh, eshdr );
				else
					unk_slisting->print("Unhandled Symbol %s type %s (name:%s)\n",sh_type_name,type_name,name);
			}
		}

		elf_end( eh );
		close( lfd );
	}

	void parse_rel( const char * st_name, Elf_Data * ed , Elf * e , Elf32_Shdr * shdr )
	{
		parse_overview->print("In parse rel for %s, links %i\n", st_name, shdr->sh_link );
		if( ed->d_type != ELF_T_REL )
		{
			parse_error->print("parse_rel: section data unknown type (%s)\n", stype_to_string(ed->d_type));
			return;
		}
		int total = ed->d_size / sizeof( Elf32_Rel );
		if( ed->d_size % sizeof( Elf32_Rel ) != 0 )
		{
			parse_error->print("parse_rel: Unknown junk at the end of (%s)\n", stype_to_string(ed->d_type));
			return;
		}
		parse_overview->print("%i relocations.\n", total );
		Elf32_Rel * rel = (Elf32_Rel*)ed->d_buf;
		Elf32_Rel * lastrel = (Elf32_Rel*)( (char*)ed->d_buf + ed->d_size);
		int num = 0;
		for( ; rel < lastrel ; rel++, num++ )
		{
			int symtype = ELF32_R_TYPE( rel->r_info );
			int sym = ELF32_R_SYM( rel->r_info );
			parse_overview->print("%05i:  % 4i % 8i 0x%08x\n",num, symtype, sym,
				rel->r_offset);
		}
	}

	void parse_string_table( const char * st_name, Elf_Data * ed , Elf * e , Elf32_Shdr * shdr )
	{
		if( ed->d_type != ELF_T_BYTE )
		{
			parse_error->print("parse_string_table: section data unknown type (%s)\n", stype_to_string(ed->d_type));
			return;
		}
		unk_slisting->print("STRTAB section %s not parsed\n",st_name);
	}

	void parse_bss( const char * st_name, Elf_Data * ed , Elf * e , Elf32_Shdr * shdr )
	{
		parse_overview->print("NOBITS section %s not parsed yet.\n",st_name);
	}

	void parse_gnu_verneed( const char * st_name, Elf_Data * ed , Elf * e , Elf32_Shdr * shdr )
	{
		if( ed->d_type != ELF_T_VNEED )
		{
			parse_error->print("parse_gnu_verneed: section data unknown type %s\n", stype_to_string(ed->d_type));
			return;
		}

		vneed_parse->print("VN: section size %i, Verdef size = %i\n",ed->d_size, sizeof(Elf32_Verneed));
		Elf32_Verneed * need = (Elf32_Verneed*)ed->d_buf;
		Elf32_Verneed * lastneed = (Elf32_Verneed*)( (char*)ed->d_buf + ed->d_size);
		vneed_parse->print( "============= % 0s VERSION NEEDED TABLE =============\n",st_name);

		Elf32_Vernaux * auxneed;
		int num = 0;
		const char * name;
		for( ; need < lastneed; need++, num++ )
		{
			const char * need_type = NULL;
			switch( need->vn_version )
			{
				case VER_NEED_NONE: need_type = "None"; break;
				case VER_NEED_CURRENT: need_type = "Current"; break;
				case VER_NEED_NUM: need_type = "Given Number"; break;
				default: need_type="UNKNOWN";
			}
			vneed_parse->print("%s %i aux, file %i, vnaux off 0x%04x, next vneed @ 0x%04x\n",
			 need_type, need->vn_cnt, need->vn_file, need->vn_aux, need->vn_next);
			Elf32_Word next_vn = need->vn_aux;
			void * vn_obj_base = (void*)need;
			while( next_vn )
			{
				const char * aux_flags = "[none]";
				auxneed = (Elf32_Vernaux*)(((char*)vn_obj_base)+need->vn_aux);
				if( auxneed->vna_flags != 0 )
					if( auxneed->vna_flags == VER_FLG_WEAK )
						aux_flags = "(weak)";
					else
						aux_flags = "UNKNOWN";

				vneed_parse->print("-- aux: hash 0x%08x flags: %s name; %s next @ 0x%04x\n",
					auxneed->vna_hash, aux_flags,
					 elf_strptr( e, shdr->sh_link, (size_t)auxneed->vna_name), auxneed->vna_next);
				next_vn = auxneed->vna_next;
				vn_obj_base = auxneed;
			}
			need = (Elf32_Verneed*)vn_obj_base;

		}

		parse_overview->print("GNU_verneed section %i entries\n",num);
	}

	void parse_gnu_version( const char * st_name, Elf_Data * ed , Elf * e , Elf32_Shdr * shdr )
	{
		unk_slisting->print("GNU_versym section  type %s\n",stype_to_string( ed->d_type));
		unk_slisting->print("section is %i bytes.\n",ed->d_size);
		
	}

	void parse_progbits( const char * st_name, Elf_Data * ed , Elf * e , Elf32_Shdr * shdr )
	{
		static bool notified = false;
		if( ! notified )
		{
			notified = true;
			 unk_slisting->print("PROGBITS Not Parsed: first section %s \n",st_name);
		}
	}

	void parse_dyn( const char * st_name, Elf_Data * ed , Elf * e , Elf32_Shdr * shdr )
	{
		int sym_count = 0;
#define BIND_TYPE( S , T ) (ELF32_ST_BIND( S->st_info) == T )
		Elf32_Dyn * sym = (Elf32_Dyn*)ed->d_buf;
		Elf32_Dyn * lastsym = (Elf32_Dyn*)( (char*)ed->d_buf + ed->d_size);

		int num = 0;

		for( ; sym < lastsym; sym++, num++ )
		{
			const char * stype;
			if( sym->d_tag == DT_NULL ) continue;
			stype = dynamic_type_to_string( sym->d_tag );
			switch( sym->d_tag )
			{
				case DT_NEEDED:
				dynsec->print("Requires: %s\n",
				 elf_strptr(e, shdr->sh_link, (size_t) sym->d_un.d_val ));
				break;
				case DT_SONAME:
				dynsec->print("SoName: %s\n",
				 elf_strptr(e, shdr->sh_link, (size_t) sym->d_un.d_val ));
				break;
				case DT_INIT:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("Init: @ 0x%08x\n", sym->d_un.d_ptr);
				break;
				case DT_FINI:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("Fini: @ 0x%08x\n", sym->d_un.d_ptr);
				break;
				case DT_GNU_HASH:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("GnuHash: @ 0x%08x\n", sym->d_un.d_ptr);
				break;
				case DT_STRTAB:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("StringTable: @ 0x%08x\n", sym->d_un.d_ptr);
				break;
				case DT_SYMTAB:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("Symboltable: @ 0x%08x\n", sym->d_un.d_ptr);
				break;
				case DT_STRSZ:
				if( sym->d_un.d_val != 0 )
				dynsec->print("StringTable.Size: % 6i bytes\n", sym->d_un.d_val);
				break;
				case DT_SYMENT:
				if( sym->d_un.d_val != 0 )
				dynsec->print("SymbolTable.ElementSize: % 8i bytes\n", sym->d_un.d_val);
				break;
				case DT_PLTGOT:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("PLTGOT?:  0x%08x\n", sym->d_un.d_ptr);
				break;
				case DT_PLTREL:
				if( sym->d_un.d_val != 0 )
				dynsec->print("PLT Relocations Type:  % 6i\n", sym->d_un.d_val);
				break;
				case DT_JMPREL:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("PLT Relocations Location:  0x%08i\n", sym->d_un.d_ptr);
				break;
				case DT_PLTRELSZ:
				if( sym->d_un.d_val != 0 )
				dynsec->print("PLT Relocations Size:  % 6i bytes\n", sym->d_un.d_val);
				break;
				case DT_REL:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("REL Relocations Location:  0x%08x\n", sym->d_un.d_ptr);
				break;
				case DT_RELSZ:
				if( sym->d_un.d_val != 0 )
				dynsec->print("REL Relocations Size:  % 6i bytes\n", sym->d_un.d_val);
				break;
				case DT_RELENT:
				if( sym->d_un.d_val != 0 )
				dynsec->print("REL.ElementSize: % 8i bytes\n", sym->d_un.d_val);
				break;
				case DT_VERNEED:
				if( sym->d_un.d_ptr != 0 )
				dynsec->print("VERNEED Location:  0x%08x\n", sym->d_un.d_ptr);
				break;
				case DT_VERNEEDNUM:
				if( sym->d_un.d_val != 0 )
				dynsec->print("VERNEED Count:  % 6i\n", sym->d_un.d_val);
				break;
				case DT_VERSYM:
				if( sym->d_un.d_val != 0 )
				dynsec->print("VERSYM :  %08x\n", sym->d_un.d_val);
				break;
				case DT_RELCOUNT:
				if( sym->d_un.d_val != 0 )
				dynsec->print("REL.Count :  % 6i items\n", sym->d_un.d_val);
				break;
				case DT_CHECKSUM:
				if( sym->d_un.d_val != 0 )
				dynsec->print("Checksum :  %08x\n", sym->d_un.d_val);
				break;
				case DT_GNU_PRELINKED:
				if( sym->d_un.d_val != 0 )
				dynsec->print("Prelink.timestamp :  %s\n", ctime((time_t*)&sym->d_un.d_val));
				break;
				default:
				dynsec->print("UNKNOWN: %05i:  % 14s\n",num,stype);
			}
		}

		parse_overview->print("ParseDyn: %s: %i symbols\n",st_name,num );
	}

	void parse_note( const char * st_name, Elf_Data * ed , Elf * e , Elf32_Shdr* shdr  )
	{
		struct gnu_abi_note
		{
			Elf32_Word os_desc, major_abi,minor_abi,sub_abi;
		};

		gnu_abi_note * gn;

		Elf32_Nhdr * n = (Elf32_Nhdr*)ed->d_buf;
		othr_sects->print("Parsing %s NOTE\n", st_name );
		othr_sects->print("Parsing NHDR (%i name bytes, %i descbytes, type %i)\n", n->n_namesz, n->n_descsz, n->n_type);
		if( n->n_type > 3 )
		{
			parse_error->print("NHDR type not valid for object file\n");
			return;
		}
		char * name = ((char*)ed->d_buf)+sizeof(Elf32_Nhdr);

		if( strcmp( name , "GNU" ) == 0)
		{
			switch( n->n_type )
			{
				case NT_GNU_ABI_TAG:
		gn = (gnu_abi_note*)((char*)ed->d_buf)+sizeof(Elf32_Nhdr)+ n->n_namesz;
					parse_error->print("NHDR GNU Abi Tag not parsed\n");
					break;
				case NT_GNU_HWCAP:
					parse_error->print("NHDR GNU hwcap Tag not parsed\n");
					break;
				case NT_GNU_BUILD_ID:
				{
					int bits = n->n_descsz * 8;
					const char * id_type;
					if( bits  == 160 )
						id_type = "(sha1?)";
					else if( bits == 128 )
						id_type = "(random or md5?)";
					else
						id_type = "(custom)";
					parse_overview->print("Has %i bit %s Build Id.\n",
					bits,id_type);
					break;
				}
			}
		}
		othr_sects->print("%s Note Type is:[%s]\n",st_name,name);

	}

	void parse_symbol_table( const char * st_name , Elf_Data * ed, Elf * e, Elf_Scn * sec )
	{
		Elf32_Shdr *shdr = elf32_getshdr( sec );
		SymbolTableHead * sth;
		int sym_count = 0;
#define BIND_TYPE( S , T ) (ELF32_ST_BIND( S->st_info) == T )
		Elf32_Sym * sym = (Elf32_Sym*)ed->d_buf;
		Elf32_Sym * lastsym = (Elf32_Sym*)( (char*)ed->d_buf + ed->d_size);
		int div_count = ed->d_size / sizeof( Elf32_Sym );
		Elf32_Word * table = (Elf32_Word*)malloc( sizeof( Elf32_Word ) * div_count );
		symtab->print( "================= % 0s SYMBOL TABLE =================\n",st_name);
		int num = 0;
		const char * name;
		for( ; sym < lastsym; sym++, num++ )
		{
			if( num < div_count ) table[num] = sym->st_name;
			if( (sym->st_value == 0 ) )
				continue;
			name = elf_strptr(e, shdr->sh_link, (size_t) sym->st_name );
			const char * sbind = "   ";
			if( BIND_TYPE(sym,STB_WEAK)) sbind = "(w)";
			else if( BIND_TYPE(sym,STB_GLOBAL)) sbind = "(G)";
			else if( BIND_TYPE(sym,STB_LOCAL)) sbind = "(l)";
			else if( BIND_TYPE(sym,STB_NUM)) sbind = "(N)";

			const char * stype = "unk";
			if( BIND_TYPE( sym, STT_NOTYPE )) stype ="none";
			else if( BIND_TYPE( sym, STT_OBJECT )) stype ="obj";
			else if( BIND_TYPE( sym, STT_FUNC )) stype ="func";
			else if( BIND_TYPE( sym, STT_SECTION )) stype ="sctn";
			else if( BIND_TYPE( sym, STT_FILE )) stype ="file";
			else if( BIND_TYPE( sym, STT_COMMON )) stype ="cmon";
			else if( BIND_TYPE( sym, STT_TLS )) stype ="tls";
			else if( BIND_TYPE( sym, STT_NUM )) stype ="NUM";
			symtab->print("%05i:  % 48s % 3s % 4s\n",num,name,sbind,stype);
		}
		sym_count = num;
		symtab->print( "================ % 0s FUNCTION TABLE ================\n",st_name);
		sym = (Elf32_Sym*)ed->d_buf;
		num = 0;
		for( ; sym < lastsym; sym++ , num ++ )
		{
			// exclue all unknown, weak , non-functions.
			if( (sym->st_value == 0 ) ||
			    ! (BIND_TYPE( sym, STT_FUNC )))
				continue;
			name = elf_strptr(e, shdr->sh_link, (size_t) sym->st_name );
			symtab->print("%05i:  % 48s\n",num,name);
		}
		symtab->print("=======================================================\n");

		parse_overview->print("%s symbol table has %i entires.\n",st_name,sym_count);
		sth  =  workingLibrary->addSymbolTable( st_name, sym_count,elf_ndxscn( sec ));
		sth->table = table;
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
			parse_error->print("I:L:parse_gnu_hash: maskwords malformed ( not power of 2 )\n");
			return;
		}

		uint32_t * bloom_filter = array+4;
		uint32_t * hash_buckets = array + 4 + maskwords;
		uint32_t * hash_values = array + 4 + nbuckets + maskwords;

		gnuhash->print( "======== GNU_HASH ========\n"
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

		parse_overview->print("has gnuhash lookup with %i byte bloom filter\n", maskwords);
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
#define DT(D) case DT_##D: ret = #D; break;
	const char * dynamic_type_to_string( Elf32_Sword type )
	{
		static char unkbuf[64];
		const char * ret;
		switch( type )
		{
			DT(NULL); DT(NEEDED); DT(PLTRELSZ); DT(PLTGOT); DT(HASH);
			DT(STRTAB); DT(SYMTAB); DT(RELA); DT(RELASZ); DT(RELAENT);
			DT(STRSZ); DT(SYMENT); DT(INIT); DT(FINI); DT(SONAME);
			DT(RPATH); DT(SYMBOLIC); DT(REL);DT(RELSZ); DT(RELENT);
			DT(PLTREL); DT(DEBUG); DT(TEXTREL); DT(JMPREL); DT(BIND_NOW);
			DT(INIT_ARRAY); DT(FINI_ARRAY); DT(INIT_ARRAYSZ);
			DT(FINI_ARRAYSZ); DT(RUNPATH); DT(FLAGS); DT(PREINIT_ARRAY);
			DT(PREINIT_ARRAYSZ); DT(GNU_PRELINKED); DT(GNU_CONFLICTSZ);
			DT(GNU_LIBLISTSZ); DT(CHECKSUM); DT(PLTPADSZ);DT(MOVEENT);
			DT(MOVESZ); DT(SYMINSZ); DT(SYMINENT); DT(FEATURE_1); DT(POSFLAG_1);
			DT(GNU_HASH); DT(TLSDESC_PLT); DT(TLSDESC_GOT); DT(GNU_CONFLICT);
			DT(GNU_LIBLIST); DT(CONFIG); DT(DEPAUDIT); DT(AUDIT); DT(PLTPAD);
			DT(MOVETAB); DT(SYMINFO); DT(VERSYM); DT(RELACOUNT); DT(RELCOUNT);
			DT(VERDEF);DT(VERDEFNUM); DT(VERNEED); DT(VERNEEDNUM);

			default:
				snprintf( unkbuf,64,"unknown(%i)",type);
				ret = unkbuf;//"unknown";
				break;
		}
		return ret;
	}
#define SHT(T) case SHT_##T: ret = #T; break;
	const char * shtype_to_string( Elf32_Word type )
	{
		const char * ret;
		switch( type )
		{
			SHT(NULL); SHT(PROGBITS); SHT(SYMTAB);  SHT(STRTAB); SHT(RELA); SHT(HASH); SHT(DYNAMIC);
			SHT(NOTE); SHT(NOBITS);  SHT(REL);  SHT(SHLIB);  SHT(DYNSYM); SHT(INIT_ARRAY); SHT(FINI_ARRAY);
			SHT(PREINIT_ARRAY); SHT(GROUP); SHT(SYMTAB_SHNDX); SHT(GNU_ATTRIBUTES); SHT(GNU_HASH);
			SHT(GNU_LIBLIST); SHT(CHECKSUM); SHT(GNU_verdef); SHT(GNU_verneed); SHT(GNU_versym);

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

#define CRN(T) case NT_##T: ret = #T; break;
	const char * coretype_to_string( Elf32_Word type )
	{
		const char * ret;
		switch( type )
		{
			CRN(PRSTATUS);
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
