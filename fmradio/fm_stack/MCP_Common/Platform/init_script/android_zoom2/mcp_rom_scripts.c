/* Automatically Generated on 'Wednesday 28th of April 2010 12:00:41 PM' */

#include "mcp_hal_types.h"
#include "mcp_hal_defs.h"
#include "mcp_rom_scripts_db.h"

#define MCP_ROM_SCRIPTS_BUF_SIZE_1	((McpUint)	32)

static const McpU8 mcpRomScripts_Buf_1[MCP_ROM_SCRIPTS_BUF_SIZE_1] =
{
	0x42, 0x54, 0x53, 0x42, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

extern const McpRomScripts_Data mcpRomScripts_Data[];

const McpRomScripts_Data mcpRomScripts_Data[] = {
	{"tipex.bts", MCP_ROM_SCRIPTS_BUF_SIZE_1, mcpRomScripts_Buf_1}
};

extern const McpUint mcpRomScripts_NumOfScripts;

const McpUint mcpRomScripts_NumOfScripts = 1;

