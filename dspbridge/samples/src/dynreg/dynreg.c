/*
 *  Copyright 2001-2008 Texas Instruments - http://www.ti.com/
 * 
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */


/*
 *  ======== dynreg.c ========
 *  "dynreg" is a console utility that allows developers to
 *  register nodes that are to be dynamically loaded along with any
 *  corresponding or dependent libraries.
 *
 *  Before a dynamically loaded node can be loaded during runtime, it
 *  must first be registered in the Node Database.  This is already done
 *  on startup for statically loaded nodes.  Dynamic Node registration
 *  must be done after the dynamic base image has been loaded and started.
 *
 *  If a node library contains configurations for more than one node,
 *  all nodes will be registered with the library.
 *
 *  If "dynreg" encounters an error, it will display a DSP/BIOS Bridge GPP
 *  API error code if Verbose is enabled.
 *
 *  Usage:
 *      dynreg [optional args] <library paths>
 *
 *  Options:
 *      -v: verbose mode.
 *	-ls: Short list of currently registered node in the Node Database
 *	-ln: Long list of currently registered node in the Node Database
 *	-r <lib_path>: Register dynamic library
 *	-u <lib_path>: Unregister dynamic library
 *      -?: displays "dynreg" usage.
 *
 *  Example:
 *      1.  Load and start an OEM DSP base image
 *
 *          cexec -w dynbase_tiomap1510.x55l
 *
 *	2.  Dynamically register dynamic node from node library
 *
 *	    dynreg -r "nodedyn.o55l"
 *
 *	3.  Load application which loads dynamic node
 *
 *	4.  Dynamically unregister dynamic node when finished
 *
 *	    dynreg -u "nodedyn.o55l"
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dbdefs.h>

#include <dbc.h>
#include <dbapi.h>
#include <DSPManager.h>
#include <dbdcddef.h>
#include <uuidutil.h>
#include <getsection.h>
#include "dlclasses_hdr.h"
#include <nldrdefs.h>
#include <csl.h>

typedef enum {
	DYNREG_LONGLIST,
	DYNREG_SHORTLIST,
	DYNREG_REGLIB,
	DYNREG_UREGLIB,
	DYNREG_CMDNULL
} DYNREG_COMMAND;

/* Dynreg cares about the following sections: */
#define DEPUUIDSECT ".dspbridge_uuid"	/* Extract UUID from dep lib */
#define PHASESECT ".dspbridge_psect"	/* Extract phase from dynlib */
#define DCD_REGISTER_SECTION ".dcd_register"	/* Extract UUIDs from nodelib */

#ifndef UINT32_C
#define UINT32_C(zzz) ((uint32_t)zzz)
#endif
#define DOFF_ALIGN(x) (((x) + 3) & ~UINT32_C(3))

/* Usable functions */
static VOID DisplayUsage();
static VOID PrintVerbose(PSTR pstrFmt, ...);
static CHAR *getNodeType(DSP_NODETYPE nodeType);
static int getSectDataFromLib(IN CHAR *szLibPath, IN CHAR *szSection,
					OUT UINT *pNumNodes, OPTIONAL OUT struct DSP_UUID *pLibUUIDs,
														OUT NLDR_PHASE *phase);
static int ProcessArgs(INT argc, CHAR *argv[], DYNREG_COMMAND *cmd,
														      CHAR *szLibPath);
static int readBuffer(struct Dynamic_Loader_Stream *this, void *buffer,
															unsigned bufsize);
static int setFilePosn(struct Dynamic_Loader_Stream *this, uint_least32_t pos);

/* global variables. */
bool g_fVerbose = false;

FILE *filePointer = NULL;

/*
 *  ======== main ========
 */
INT main(INT argc, CHAR *argv[])
{

	int status = 0;
	struct DSP_UUID *pLibUUIDs = NULL;
	CHAR szUuid[36];
	struct DSP_NDBPROPS hNdbProps;
	UINT uIndex = 0;
	UINT uNumNodes = 0;
	UINT i = 0;
	CHAR szTemp[DSP_MAXNAMELEN];
	CHAR szLibPath[256];
	bool fShort = false;
	bool fPhaseLib = false;
	DYNREG_COMMAND cmd;
	NLDR_PHASE phase;

	status = DspManager_Open(0, NULL);
	if (DSP_SUCCEEDED(status))
		status = ProcessArgs(argc, argv, &cmd, szLibPath);
	else {
		printf("Failed to Open DSP manager \n");
		exit(-1);
	}
	if (DSP_SUCCEEDED(status)) {
		switch (cmd) {
		case DYNREG_SHORTLIST:
			/* Brief listing of nodes */
			fShort = true;
		case DYNREG_LONGLIST:
			/* Call once to retrieve number of registered nodes */
			status = DSPManager_EnumNodeInfo(0, &hNdbProps, sizeof(hNdbProps),
																	&uNumNodes);
			printf("Registered Nodes: \n\n");
			/* Cycle through the enumeration of nodes */
			while (DSP_SUCCEEDED(status) && (uIndex < uNumNodes)) {
				status = DSPManager_EnumNodeInfo(uIndex, &hNdbProps,
												sizeof(hNdbProps),&uNumNodes);
				/* CSL_WcharToAnsi(szTemp,hNdbProps.acName,
				   CSL_Wstrlen(hNdbProps.acName)); */
				printf("Node: %s\n", szTemp);
				if (!fShort) {
					printf("Type: %s\n", getNodeType(hNdbProps.uNodeType));
					UUID_UuidToString(&(hNdbProps.uiNodeID),szUuid, MAXUUIDLEN);
					printf("UUID: %s\n\n", szUuid);
				}
				uIndex++;
			}
			if (DSP_FAILED(status)) {
				printf("Failure to enumerate nodes: 0x%x.\n",
														(unsigned int)status);
			}
			break;
		case DYNREG_REGLIB:
			/* Register Library */
			pLibUUIDs = calloc(1, sizeof(struct DSP_UUID));
			status = getSectDataFromLib(szLibPath, PHASESECT, NULL, pLibUUIDs,
																		&phase);
			if (status != -ENXIO) {
				if (DSP_FAILED(status)) {
					PrintVerbose("Unable to open %s\n",szLibPath);
					break;
				} else {
					fPhaseLib = true;
					switch (phase) {
					case NLDR_CREATE:
						PrintVerbose("Registering Create phase\n");
						status = DSPManager_RegisterObject(&pLibUUIDs[0],
												DSP_DCDCREATELIBTYPE,szLibPath);
						break;
					case NLDR_EXECUTE:
						PrintVerbose("Registering Execute phase\n");
						status = DSPManager_RegisterObject(&pLibUUIDs[0],
											DSP_DCDEXECUTELIBTYPE, szLibPath);
						break;
					case NLDR_DELETE:
						PrintVerbose("Registering Delete phase\n");
						status = DSPManager_RegisterObject(&pLibUUIDs[0],
												DSP_DCDDELETELIBTYPE,szLibPath);
						break;
					default:
						fPhaseLib = false;
						break;
					}
					if (DSP_SUCCEEDED(status)) {
						PrintVerbose("Successfully registered library.\n");
					} else {
						PrintVerbose("Failed to register library\n");
					}
				}
			}
			/* Must free here in case of reallocation later */
			/* MEM_Free(pLibUUIDs); */
			free(pLibUUIDs);
			pLibUUIDs = NULL;
			/* Get number of nodes within library; 0 = NO NODES */
			status = getSectDataFromLib(szLibPath, DCD_REGISTER_SECTION,
														&uNumNodes, NULL, NULL);
			if (status != -ENXIO) {
				PrintVerbose("Registering %u nodes\n",uNumNodes);
				if (DSP_FAILED(status)) {
					PrintVerbose("Unable to open %s\n", szLibPath);
					break;
				} else {
					/* Create struct DSP_UUID array and extract UUIDs */
					pLibUUIDs = calloc(1, sizeof(struct DSP_UUID) * uNumNodes);
					if (!pLibUUIDs) {
						PrintVerbose("Memory Allocation Error\n");
						break;
					}
					status = getSectDataFromLib(szLibPath,DCD_REGISTER_SECTION,
													&uNumNodes,pLibUUIDs,NULL);
				}
				for (i = 0;i < uNumNodes && DSP_SUCCEEDED(status);i++) {
					/* hari -- for debug */
#if 0
					printf("Dyn-Reg:Reg: Type: %s \n",
					       getNodeType(DSP_DCDNODETYPE));
					UUID_UuidToString(&(pLibUUIDs[i]),
							  szUuid, MAXUUIDLEN);
					printf("Dyn-Reg:Reg: UUID: %s \n",
					       szUuid);
					printf("Dyn-Reg:Reg: Path: %s \n \n",
					       szLibPath);
#endif
					status = DSPManager_RegisterObject(&(pLibUUIDs [i]),
													DSP_DCDNODETYPE, szLibPath);
					if (DSP_SUCCEEDED(status)) {
						/* Register library associated with node */
						status = DSPManager_RegisterObject(&pLibUUIDs[i],
												DSP_DCDLIBRARYTYPE, szLibPath);
						if (DSP_FAILED(status)) {
							PrintVerbose("Failed to register node library\n");
						} else {
#if 0
							/* hari -- for debug */
							printf("Dyn-Reg:Reg: Type: %s\n", 
											getNodeType(DSP_DCDLIBRARYTYPE));
							UUID_UuidToString(&(pLibUUIDs [i]), szUuid,
																	MAXUUIDLEN);
							printf("Dyn-Reg:Reg: UUID: %s \n", szUuid);
							printf("Dyn-Reg:Reg: Path: %s \n \n", szLibPath);
#endif
							PrintVerbose("Successfully registered node.\n");
						}
					} else {
						PrintVerbose("Failed to register node\n");
					}
				}
			} else if (!fPhaseLib) {
				/* Register Library */
				PrintVerbose("Registering Library: %s\n", szLibPath); 
				/* Only one struct DSP_UUID is needed from lib */
				pLibUUIDs = calloc(1, sizeof(struct DSP_UUID));
				if (!pLibUUIDs) {
					PrintVerbose("Memory Allocation Error\n");
					break;
				}
				status = getSectDataFromLib(szLibPath, DEPUUIDSECT, &uNumNodes,
																pLibUUIDs,NULL);
				if (DSP_FAILED(status)) {
					PrintVerbose("Unable to open %s\n", szLibPath);
					break;
				}
				status = DSPManager_RegisterObject(&(pLibUUIDs[0]),
												DSP_DCDLIBRARYTYPE, szLibPath);
				if (DSP_FAILED(status)) {
					PrintVerbose("Failed to register library\n");
				} else {
					PrintVerbose("Successfully registered library.\n");
				}
			}
			break;
		case DYNREG_UREGLIB:
			/* Unregister library */
			pLibUUIDs = calloc(1, sizeof(struct DSP_UUID));
			status = getSectDataFromLib(szLibPath, PHASESECT, NULL, pLibUUIDs,
																		&phase);
			if (status != -ENXIO) {
				if (DSP_FAILED(status)) {
					PrintVerbose("Unable to open %s\n", szLibPath);
					break;
				} else {
					fPhaseLib = true;
					switch (phase) {
					case NLDR_CREATE:
						PrintVerbose("Unegistering Create phase\n");
						status = DSPManager_UnregisterObject (&pLibUUIDs[0],
														DSP_DCDCREATELIBTYPE);
						break;
					case NLDR_EXECUTE:
						PrintVerbose("Unregistering Execute phase\n");
						status = DSPManager_UnregisterObject(&pLibUUIDs[0],
														DSP_DCDEXECUTELIBTYPE);
						break;
					case NLDR_DELETE:
						PrintVerbose("Unregistering Delete phase\n");
						status = DSPManager_UnregisterObject(&pLibUUIDs[0],
														DSP_DCDDELETELIBTYPE);
						break;
					default:
						fPhaseLib = false;
						break;
					}
					if (DSP_SUCCEEDED(status)) {
						PrintVerbose("Successfully unregistered library.\n");
					} else {
						PrintVerbose("Failed to unregister library\n");
					}
				}
			}
			/* Must free here in case of reallocation later */
			free(pLibUUIDs);
			pLibUUIDs = NULL;
			/* Get number of nodes within library */
			status = getSectDataFromLib(szLibPath, DCD_REGISTER_SECTION,
														&uNumNodes, NULL, NULL);
			if (status != -ENXIO) {
				PrintVerbose("Unregistering %u nodes\n", uNumNodes);
				if (DSP_FAILED(status)) {
					PrintVerbose("Unable to open %s\n", szLibPath);
					break;
				} else {
					/* Create struct DSP_UUID array and extract UUIDs */
					pLibUUIDs = calloc(1, uNumNodes * sizeof(struct DSP_UUID));
					if (!pLibUUIDs) {
						PrintVerbose("Memory Allocation Error\n");
						break;
					}
					status = getSectDataFromLib(szLibPath,DCD_REGISTER_SECTION,
													&uNumNodes,pLibUUIDs,NULL);
				}
				for (i = 0;i < uNumNodes && DSP_SUCCEEDED(status);i++) {
					status = DSPManager_UnregisterObject(&(pLibUUIDs[i]),
															DSP_DCDLIBRARYTYPE);
					if (DSP_SUCCEEDED(status)) {
						/* Unregister node */
						PrintVerbose ("Unregistering Library: %s\n", szLibPath);
#if 0
						/* hari -- for debug */
						printf ("Dyn-Reg: Unreg: Type: %s\n", 
											getNodeType(DSP_DCDLIBRARYTYPE));
						UUID_UuidToString(&(pLibUUIDs [i]), szUuid, MAXUUIDLEN);
						printf ("Dyn-Reg: Unreg: UUID: %s \n", szUuid);
						printf ("Dyn-Reg: Unreg: Path: %s \n \n", szLibPath);
#endif
						status = DSPManager_UnregisterObject (&(pLibUUIDs[i]),
															DSP_DCDNODETYPE);
						if (DSP_FAILED(status)) {
							PrintVerbose("Failed to unregister node\n");
						} else {
#if 0
							/* hari -- for debug */
							printf ("Dyn-Reg: Unreg: Type: %s\n",
												getNodeType(DSP_DCDNODETYPE));
							UUID_UuidToString(&(pLibUUIDs[i]),szUuid,
																	MAXUUIDLEN);
							printf("Dyn-Reg: Unreg: UUID: %s \n", szUuid);
							printf("Dyn-Reg: Unreg: Path: %s \n \n",szLibPath);
#endif
							PrintVerbose("Successfully unregistered node.\n");
						}
					} else {
						PrintVerbose("Failed to unregister node library: 0x%x\n"
																	, status);
					}
				}
			} else if (!fPhaseLib) {
				/* Unregister Library */
				PrintVerbose("Unregistering Library: %s\n", szLibPath);
				/* Only one struct DSP_UUID is needed from lib */
				pLibUUIDs = calloc(1, sizeof(struct DSP_UUID));
				if (!pLibUUIDs) {
					PrintVerbose("Memory Allocation Error\n");
					break;
				}
				status = getSectDataFromLib(szLibPath, DEPUUIDSECT, &uNumNodes,
																pLibUUIDs,NULL);
				if (DSP_FAILED(status)) {
					PrintVerbose("Unable to open %s\n", szLibPath);
					break;
				}
				status = DSPManager_UnregisterObject(&(pLibUUIDs[0]),
															DSP_DCDLIBRARYTYPE);
				if (DSP_FAILED(status)) {
					PrintVerbose("Failed to unregister library: 0x%x\n",status);
				} else {
					PrintVerbose ("Successfully unregistered library\n");
				}
			}
			break;
		default:
			break;
		}
	}
	if (pLibUUIDs) {
		free(pLibUUIDs);
	}
	/* CSL_Exit();
	   MEM_Exit();
	   KFILE_Exit();
	 */
	return (DSP_SUCCEEDED(status) ? 0 : -1);
}

/*
 *  ======== getNodeType ========
 *  Given DSP_NODETYPE, return corresponding string
 */
static CHAR *getNodeType(DSP_NODETYPE nodeType)
{
	switch (nodeType) {
	case NODE_DEVICE:
		return "NODE_DEVICE";
	case NODE_MESSAGE:
		return "NODE_MESSAGE";
	case NODE_TASK:
		return "NODE_TASK";
	case NODE_DAISSOCKET:
		return "NODE_DAISSOCKET";
	default:
		return "NULL";
	}
}

/*
 *  ======== ProcessArgs ========
 *  Process command-line arguments
 */
static int ProcessArgs(INT argc, CHAR *argv[], DYNREG_COMMAND *cmd,
																CHAR *szLibPath)
{

	INT i = 0;
	int status = 0;
	/* Initialize as null command */
	*cmd = DYNREG_CMDNULL;
	if (argc < 2 || argc > 5) {
		DisplayUsage();
		status = -EPERM;
	} else {
		while (++i < argc) {
			/* if (CSL_Strcmp(argv[i],"-v")==0) { */
			if (strcmp(argv[i], "-v") == 0) {
				/* Verbose on */
				g_fVerbose = true;
			}
			/* else if (CSL_Strcmp(argv[i],"-ln")==0) { */
			else if (strcmp(argv[i], "-ln") == 0) {
				/* List nodes */
				*cmd = DYNREG_LONGLIST;
			}
			/* else if (CSL_Strcmp(argv[i],"-ls")==0) { */
			else if (strcmp(argv[i], "-ls") == 0) {
				/* List nodes - short */
				*cmd = DYNREG_SHORTLIST;
			}
			/* else if (CSL_Strcmp(argv[i],"-r")==0) { */
			else if (strcmp(argv[i], "-r") == 0) {
				/* Register a library */
				i++;
				*cmd = DYNREG_REGLIB;
				/* CSL_Strcpyn(szLibPath,argv[i],strlen(argv[i])); */
				strncpy(szLibPath, argv[i], strlen(argv[i]));
				szLibPath[strlen(argv[i])] = '\0';
			}
			/* else if (CSL_Strcmp(argv[i],"-u")==0) { */
			else if (strcmp(argv[i], "-u") == 0) {
				/* Unregister a library */
				i++;
				*cmd = DYNREG_UREGLIB;
				/* CSL_Strcpyn(szLibPath,argv[i],strlen(argv[i])); */
				strncpy(szLibPath, argv[i], strlen(argv[i]));
				szLibPath[strlen(argv[i])] = '\0';
			} else {
				/* Print Usage */
				DisplayUsage();
				status = -EPERM;
				break;
			}
		}
	}
	return status;
}

/*
 *  ======== getSectDataFromLib ========
 *  Given a library name and section name, extract all UUIDs or phase
 *  data and return pointer to array
 */
static int getSectDataFromLib(IN CHAR *szLibPath, IN CHAR *szSection,
											OUT UINT *pNumNodes,
				OPTIONAL OUT struct DSP_UUID *pLibUUIDs,OUT NLDR_PHASE *phase)
{
	INT cResult = 0;
	INT nNodeNum = 0;
	DLOAD_mhandle desc = NULL;
	const struct LDR_SECTION_INFO *sect;
	ULONG ulAddr = 0;
	ULONG ulSize = 0;
	UINT uByteSize;
	CHAR *pszCur;
	CHAR *pToken = NULL;
	CHAR *pszCoffBuf;
	//CHAR      *pTempBuf;
	CHAR *pTempCoffBuf;
	CHAR seps[] = ", ";
	int status = 0;
	struct DL_stream_t inputStream;
	struct DL_sym_t inputSymbols;

	/* Reset count */
	if (!pLibUUIDs) {
		if (pNumNodes) {
			*pNumNodes = 0;
		}
	}

	DLsym_init(&inputSymbols);

	inputStream.dstrm.read_buffer = readBuffer;
	inputStream.dstrm.set_file_posn = setFilePosn;
	/* Open DOFF file */
	filePointer = fopen(szLibPath, "rb");
	desc = DLOAD_module_open(&(inputStream.dstrm), &(inputSymbols.sym));
	if (!desc) {
		/* Error */
		status = -EBADF;
	} else {
		uByteSize = 1;
		/* Get Section header */
		cResult = DLOAD_GetSectionInfo(desc, szSection, &sect);
		if (cResult) {
			ulAddr = sect->load_addr;
			ulSize = sect->size * uByteSize;
		} else {
			status = -ENXIO;
		}
		if (DSP_SUCCEEDED(status)) {
			/* Align size */
			ulSize = DOFF_ALIGN(ulSize);
			pszCoffBuf = calloc(1, ulSize);
			if (!pszCoffBuf) {
				status = -ENOMEM;
			}
#ifdef _DB_TIOMAP
			pTempCoffBuf = calloc(1, ulSize);
			if (!pTempCoffBuf) {
				status = -ENOMEM;
			}
#endif
			if (DSP_SUCCEEDED(status)) {
				/* Read section */
				if (!DLOAD_GetSection(desc, sect, pszCoffBuf)) {
					status = -EPERM;
				}
				if (DSP_SUCCEEDED(status)) {
					pToken = CSL_Strtokr(pszCoffBuf, seps, &pszCur);
					if (phase) {
						/* Extract phase information */
						UUID_UuidFromString(pToken, &(pLibUUIDs [0]));
						pToken = CSL_Strtokr(NULL, seps, &pszCur);
						if (!CSL_Strcmp(pToken, "create")) {
							*phase = NLDR_CREATE;
						} else
						    if (!CSL_Strcmp(pToken, "execute")) {
							*phase = NLDR_EXECUTE;
						}
						/* TODO -- temporary fix needs to check why we don't
						 * see the token 'execute' */
						else if (!CSL_Strcmp(pToken, "execute1")) {
							*phase = NLDR_EXECUTE;
						} else
						    if (!CSL_Strcmp(pToken, "delete")) {
							*phase = NLDR_DELETE;
						} else {
							DBC_Assert(false);
						}
					} else {
						while (pToken != NULL) {
							if (!pLibUUIDs) {
								/* increment node number */
								(*pNumNodes)++;
							} else {
								/* Extract last string token == my UUID */
								UUID_UuidFromString(pToken, 
														&(pLibUUIDs[nNodeNum]));
								nNodeNum++;
							}
							pToken = CSL_Strtokr(NULL, seps, &pszCur);
						}
					}
				}
			}
			if (pszCoffBuf) {
				/* MEM_Free(pszCoffBuf); */
				free(pszCoffBuf);
#ifdef _DB_TIOMAP
			if(pTempCoffBuf) {
			free(pTempCoffBuf);
			}
#endif
			}
		}
		if (desc) {
			DLOAD_module_close(desc);
			desc = NULL;
		}
		if (filePointer) {
			fclose(filePointer);
			filePointer = NULL;
		}
	}
	return status;
}


/*
 *  ======== DisplayUsage ========
 *  Display usage of dynreg utility
 */
VOID DisplayUsage()
{
	fprintf(stdout, "Usage: dynreg [optional args] <library paths>\n");
	fprintf(stdout, "[optional args]:\n");
	fprintf(stdout, "-v: verbose mode.\n");
	fprintf(stdout, "-ls: Short list of currently registered nodes in \n");
	fprintf(stdout, "     the DCD Node Database.\n");
	fprintf(stdout, "-ln: Long list of currently registered nodes in \n");
	fprintf(stdout, "     the DCD Node Database.\n");
	fprintf(stdout, "-r <lib-path>: Register dynamic node \n");
	fprintf(stdout, "     and dyn node library.\n");
	fprintf(stdout, "-r <lib-path>: Register \n");
	fprintf(stdout, "     dependent dynamic library.\n");
	fprintf(stdout, "-u <lib-path>: Unregister dynamic node \n");
	fprintf(stdout, "     and dyn node library.\n");
	fprintf(stdout, "-u <lib-path>: Unregister dependent \n");
	fprintf(stdout, "     dynamic library.\n");
	fprintf(stdout, "-?: displays \"dynreg\" usage. \n");

	fprintf(stdout, "\nExample: dynreg -r \"windows\\strmcopydyn.o55l\"\n\n");
}

/*
 * ======== PrintVerbose ========
 */
VOID PrintVerbose(PSTR pstrFmt, ...)
{
	va_list args;

	if (g_fVerbose) {
		va_start(args, pstrFmt);

		vfprintf(stdout, pstrFmt, args);
		va_end(args);

		fflush(stdout);
	}
}

static int readBuffer(struct Dynamic_Loader_Stream *this,void *buffer,
															unsigned bufsize)
{
	int bytesRead = 0;

	bytesRead = fread(buffer, 1, bufsize, filePointer);

	return bytesRead;
}

static int setFilePosn(struct Dynamic_Loader_Stream *this, uint_least32_t pos)
{
	int status = 0;
	status = fseek(filePointer, (long) pos, SEEK_SET);

	return status;
}

