/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/** 
 *  \file   bmtrace.h 
 *  \brief  Event trace tool
 * 
 * This file contains trace tool function specification
 * 
 *  \see    bmtrace.c
 */


#ifndef __BM_TRACER_H
#define __BM_TRACER_H

#define CL_TRACE_CONTEXT_TASK	"Task"
#define CL_TRACE_CONTEXT_ISR	"ISR"
#define CL_TRACE_CONTEXT_SYSTEM	"Sys"

#define CL_TRACE_START() \
	unsigned long in_ts = bm_act_trace_in();

#define CL_TRACE_RESTART_Lx() \
	in_ts = bm_act_trace_in();

#ifdef TI_BMTRACE
	#define CL_TRACE_INIT(hMcpf)    bm_init(hMcpf)
	#define CL_TRACE_DEINIT()     	bm_deinit()
	#define CL_TRACE_ENABLE()   	bm_enable()
	#define CL_TRACE_DISABLE()  	bm_disable()
	#define CL_TRACE_PRINT(buf) 	bm_print_out_buffer(buf)
	#define CL_TRACE_SAVE()			bm_save_to_file()
	#define CL_TRACE_START_L1() 	CL_TRACE_START()
	#define CL_TRACE_RESTART()  	CL_TRACE_RESTART_Lx()
	#define CL_TRACE_END_L1(mod, cntxt, grp, sufx) \
		{ \
			static int loc; \
			if (unlikely(loc == 0)) \
				loc = bm_act_register_event((mod), (cntxt), (grp), 1, (char*)__FUNCTION__, (sufx), 0); \
			bm_act_trace_out(loc, in_ts); \
		}
	#define CL_TRACE_TASK_DEF() 	McpU32	in_ts; McpS32  loc=0;
	#define CL_TRACE_TASK_START() 	in_ts = bm_act_trace_in();
	#define CL_TRACE_TASK_END(mod, cntxt, grp, sufx) \
			if (unlikely(loc == 0)) \
				loc = bm_act_register_event((mod), (cntxt), (grp), 1, (char*)__FUNCTION__, (sufx), 0); \
			bm_act_trace_out(loc, in_ts);
#else
    #define CL_TRACE_INIT(hMcpf)   
    #define CL_TRACE_DEINIT()
    #define CL_TRACE_ENABLE() 
    #define CL_TRACE_DISABLE()
    #define CL_TRACE_RESTART() 
    #define CL_TRACE_PRINT(buf)  
	#define CL_TRACE_SAVE()
	#define CL_TRACE_START_L1() 
	#define CL_TRACE_END_L1(mod, cntxt, grp, sufx) 
	#define CL_TRACE_TASK_DEF()
	#define CL_TRACE_TASK_START()
	#define CL_TRACE_TASK_END(mod, cntxt, grp, sufx)
#endif


/** 
 * \fn     bm_init
 * \brief  Tracer initialization
 * 
 * This function initializes the tracer object
 * 
 * \note
 * \param	hMcpf - handle to MCPF
 * \return 	void
 * \sa     	bm_deinit
 */
void 		bm_init (handle_t hMcpf);

/** 
 * \fn     bm_deinit
 * \brief  Tracer de-initialization
 * 
 * This function de-initializes the tracer object
 * 
 * \note
 * \return 	void
 * \sa     	bm_deinit
 */
void		bm_deinit (void);

/** 
 * \fn     bm_enable
 * \brief  Enable tracing
 * 
 * This function enable logging of events into the trace buffer
 * 
 * \note
 * \return 	void
 * \sa     	bm_disable
 */
void		bm_enable (void);

/** 
 * \fn     bm_disable
 * \brief  Disable tracing
 * 
 * This function disables logging of events into the buffer
 * 
 * \note
 * \return 	void
 * \sa     	bm_enable
 */
void		bm_disable(void);

/** 
 * \fn     bm_act_trace_in
 * \brief  Start of event logging
 * 
 * This function starts specific event logging and returns time stamp
 * 
 * \note
 * \return 	void
 * \sa     	bm_act_trace_out
 */
unsigned long   bm_act_trace_in (void);

/** 
 * \fn     bm_act_trace_out
 * \brief  Endx of event logging
 * 
 * This function starts specific event logging
 * 
 * \note
 * \param	event_id - event ID
 * \param	in_ts    - timestamp of event start
 * \return 	void
 * \sa     	bm_act_trace_in
 */
void            bm_act_trace_out (int event_id, unsigned long in_ts);

/** 
 * \fn     bm_act_register_event
 * \brief  Register event
 * 
 * This function registeres event in tracer and adds event description
 * 
 * \note
 * \param	module  - module name of event
 * \param	context - context name of event
 * \param	group   - group name of event
 * \param	level   - event level number
 * \param	name    - event name
 * \param	suffix  - event name suffix
 * \param	is_param  - is parameter is used instead of end timestamp
 * \return 	event id of registered event
 * \sa     	bm_act_trace_in
 */
int             bm_act_register_event (char* module, 
									   char* context, 
									   char* group, 
									   unsigned char level, 
									   char* name, 
									   char* suffix, 
									   int is_param);

/** 
 * \fn     bm_print_out_buffer
 * \brief  Output trace buffer
 * 
 * This function produces formated text output of trace buffer to specified buffer
 * 
 * \note
 * \param	pBuf - output buffer
 * \sa     	bm_save_to_file
 */
int             bm_print_out_buffer (char *pBuf);

/** 
 * \fn     bm_save_to_file
 * \brief  Save trace buffer to file
 * 
 * This function saves the trace buffer to text file
 * 
 * \note
 * \sa     	bm_print_out_buffer
 */
void            bm_save_to_file ( void );


#endif /* _TRACER_H */
