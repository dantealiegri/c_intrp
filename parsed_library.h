#include <glibmm/ustring.h>
#include <glib/gprintf.h>

namespace Interpreter {

struct SymbolTableHead
{
	int index_number;
	int symbol_count;
	int string_table_id;
	void * table;
};

class ParsedLibrary
{
	Glib::ustring identifier;
	GHashTable * symbolIndicies; // char * , SymbolTableHead *
	public:

	int bits; // 32 or 64, now.
	int machine; // EM_* from elf.h
	ParsedLibrary( Glib::ustring _identifier )
	{
		symbolIndicies = g_hash_table_new( g_str_hash , g_str_equal );

		identifier = _identifier;
	}

	~ParsedLibrary()
	{
	}

	SymbolTableHead* getSymbolTable( Glib::ustring sym_ident )
	{
		return (SymbolTableHead*)g_hash_table_lookup( symbolIndicies , sym_ident.c_str());
	}

	// returns NULL or exists, or error.
	//  else, the new index.
	SymbolTableHead * addSymbolTable( const gchar * sym_ident, int count, int index )
	{
		SymbolTableHead * newST;
		if( ( newST = (SymbolTableHead*)g_hash_table_lookup( symbolIndicies , sym_ident)) != NULL )
		{
			return NULL; 
		}
		newST = (SymbolTableHead*)malloc(sizeof(SymbolTableHead));
		memset( newST, 0, sizeof(SymbolTableHead));
		newST->index_number = index;
		newST->symbol_count = count;
		g_hash_table_insert( symbolIndicies, strdup( sym_ident ), newST );
		return newST;
	}
};

}
