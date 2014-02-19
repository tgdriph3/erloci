/* Copyright 2012 K2Informatics GmbH, Root Laengenbold, Switzerland
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
#include <stdio.h>
#include <stdlib.h>

#include "command.h"
#include "ocisession.h"

#include "transcoder.h"
#include "term.h"

port & command::p			= port::instance();
transcoder & command::tc	= transcoder::instance();

bool command::change_log_flag(term & t)
{
    bool ret = false;
	term resp;

	if((t.lt.size() - 1) != CMD_ARGS_COUNT(RMOTE_MSG)) {
		resp.tuple()
			.add(t.lt[0])
			.add(RMOTE_MSG)
			.add(term().tuple()
						.add(term().atom("error"))
						.add(term().atom("badarg")));
		REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

    // {undefined, RMOTE_MSG, DBG_FLAG_OFF/DBG_FLAG_ON}
    if(t.lt[2].is_any_int()) {
		unsigned int log = t.lt[2].v.ui;

        switch(log) {
        case DBG_FLAG_OFF:
            REMOTE_LOG(INF, "Disabling logging...");
            log_flag = false;
            REMOTE_LOG(CRT, "This line will never show up!!\n");
			resp.tuple()
				.add(t.lt[0])
				.add(RMOTE_MSG)
				.add(term().atom("log_disabled"));
            break;
        case DBG_FLAG_ON:
            log_flag = true;
			resp.tuple()
				.add(t.lt[0])
				.add(RMOTE_MSG)
				.add(term().atom("log_enabled"));
			REMOTE_LOG(INF, "Enabled logging...");
            break;
        default:
			resp.tuple()
				.add(t.lt[0])
				.add(RMOTE_MSG)
				.add(term().tuple()
						.add(term().atom("error"))
						.add(term().atom("badarg")));
			REMOTE_LOG(ERR, "ERROR badarg %d\n", log);
            break;
        }
    } else {
//		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(resp.is_undef()) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(p.write_cmd(tc.encode(resp)) <= 0)
        ret = true;

	return ret;
}

bool command::change_log_flag(ETERM * command)
{
    bool ret = false;
    ETERM *resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(RMOTE_MSG), command, args);

	if(ARG_COUNT(command) != CMD_ARGS_COUNT(RMOTE_MSG)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], RMOTE_MSG);
		REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

    // Args : {log, DBG_FLAG_OFF/DBG_FLAG_ON}
    if(ERL_IS_INTEGER(args[1])) {
		unsigned int log = ERL_INT_UVALUE(args[1]);

        switch(log) {
        case DBG_FLAG_OFF:
            REMOTE_LOG(INF, "Disabling logging...");
            log_flag = false;
            REMOTE_LOG(CRT, "This line will never show up!!\n");
            resp = erl_format((char*)"{~w,~i,log_disabled}", args[0], RMOTE_MSG);
            break;
        case DBG_FLAG_ON:
            log_flag = true;
            resp = erl_format((char*)"{~w,~i,log_enabled}", args[0], RMOTE_MSG);
            REMOTE_LOG(INF, "Enabled logging...\n");
            break;
        default:
            resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], RMOTE_MSG);
			REMOTE_LOG(ERR, "ERROR badarg %d\n", log);
            break;
        }
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(RMOTE_MSG), args);

	return ret;
}

bool command::get_session(ETERM * command)
{
    bool ret = false;
	ETERM *resp = NULL;
	term rept;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(GET_SESSN), command, args);

	if(ARG_COUNT(command) != CMD_ARGS_COUNT(GET_SESSN)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], GET_SESSN);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg %s expected %d, got %d\n", CMD_NAME_STR(GET_SESSN), CMD_ARGS_COUNT(GET_SESSN), ARG_COUNT(command));
		ret = true;
	    goto error_exit;
	}

	// Args : Connection String, User name, Password
    if(ERL_IS_BINARY(args[1]) &&
       ERL_IS_BINARY(args[2]) &&
       ERL_IS_BINARY(args[3])) {

		   try {
				ocisession * conn_handle = new ocisession(
					(char*)ERL_BIN_PTR(args[1]), ERL_BIN_SIZE(args[1]),		// Connect String
					(char*)ERL_BIN_PTR(args[2]), ERL_BIN_SIZE(args[2]),		// User Name String
					(char*)ERL_BIN_PTR(args[3]), ERL_BIN_SIZE(args[3]));	// Password String
		        REMOTE_LOG(INF, "got connection %lu\n", (unsigned long long)conn_handle);
				ETERM *conh = erl_mk_ulonglong((unsigned long long)conn_handle);
				resp = erl_format((char*)"{~w,~i,~w}", args[0], GET_SESSN, conh);
				erl_free_term(conh);
				rept.tuple()
					.add((int)GET_SESSN)
					.add((unsigned long long)conn_handle);
		   } catch (intf_ret r) {
				resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], GET_SESSN, r.gerrcode, r.gerrbuf);
				if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
		   } catch (string str) {
				resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], GET_SESSN, str.c_str());
				if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		   } catch (...) {
				resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], GET_SESSN);
				if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		   }
	} else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(GET_SESSN), args);

	return ret;
}

bool command::release_conn(ETERM * command)
{
    bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(PUT_SESSN), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(PUT_SESSN)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], PUT_SESSN);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

	if(ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1])) {

		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
													? ERL_INT_VALUE(args[1])
													: ERL_LL_UVALUE(args[1]));
		try {
			delete conn_handle;
            resp = erl_format((char*)"{~w,~i,ok}", args[0], PUT_SESSN);
		} catch (intf_ret r) {
            resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], PUT_SESSN, r.gerrcode, r.gerrbuf);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
		} catch (string str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], PUT_SESSN, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], PUT_SESSN);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(PUT_SESSN), args);

	return ret;
}

bool command::commit(ETERM * command)
{
    bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(CMT_SESSN), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(CMT_SESSN)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], CMT_SESSN);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

	if(ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1])) {

		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
													? ERL_INT_VALUE(args[1])
													: ERL_LL_UVALUE(args[1]));
		try {
			conn_handle->commit();
            resp = erl_format((char*)"{~w,~i,ok}", args[0], CMT_SESSN);
		} catch (intf_ret r) {
            resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], CMT_SESSN, r.gerrcode, r.gerrbuf);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
		} catch (string str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], CMT_SESSN, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], CMT_SESSN);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(CMT_SESSN), args);

	return ret;
}

bool command::rollback(ETERM * command)
{
    bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(RBK_SESSN), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(RBK_SESSN)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], RBK_SESSN);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

	if(ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1])) {

		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
													? ERL_INT_VALUE(args[1])
													: ERL_LL_UVALUE(args[1]));
		try {
			conn_handle->rollback();
            resp = erl_format((char*)"{~w,~i,ok}", args[0], RBK_SESSN);
		} catch (intf_ret r) {
            resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], RBK_SESSN, r.gerrcode, r.gerrbuf);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
		} catch (string str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], RBK_SESSN, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], RBK_SESSN);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(RBK_SESSN), args);

	return ret;
}

bool command::describe(ETERM * command)
{
	bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(CMD_DSCRB), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(CMD_DSCRB)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], CMD_DSCRB);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

    // Args: Conn Handle, Describe Object string, Describe Object Type
    if((ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1]))
		&& ERL_IS_BINARY(args[2])
		&& (ERL_IS_INTEGER(args[3]) || ERL_IS_UNSIGNED_LONGLONG(args[3]) || ERL_IS_LONGLONG(args[3]))) {

		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
													? ERL_INT_VALUE(args[1])
													: ERL_LL_UVALUE(args[1]));
		unsigned char desc_typ = (unsigned char)(ERL_IS_INTEGER(args[3])
													? ERL_INT_VALUE(args[3])
													: ERL_LL_UVALUE(args[3]));
		ETERM *describes = NULL;
		try {
	        conn_handle->describe_object(ERL_BIN_PTR(args[2]), ERL_BIN_SIZE(args[2]), desc_typ, &describes, append_desc_to_list);
			if (describes)
				resp = erl_format((char*)"{~w,~i,{desc,~w}}", args[0], CMD_DSCRB, describes);
			else
				resp = erl_format((char*)"{~w,~i,{desc,[]}}", args[0], CMD_DSCRB);
		} catch (intf_ret r) {
			resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], CMD_DSCRB, r.gerrcode, r.gerrbuf);
			if (r.fn_ret == CONTINUE_WITH_ERROR) {
				if(!resp) REMOTE_LOG(INF, "Continue with ERROR Execute SQL \"%.*s;\" -> %s\n", ERL_BIN_SIZE(args[2]), ERL_BIN_PTR(args[2]), r.gerrbuf);
			} else {
				if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
				ret = true;
			}
		} catch (string str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], CMD_DSCRB, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], CMD_DSCRB);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(CMD_DSCRB), args);

	return ret;
}

bool command::prep_sql(ETERM * command)
{
	bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(PREP_STMT), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(PREP_STMT)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], PREP_STMT);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

    // Args: Conn Handle, Sql String
    if((ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1])) &&
       ERL_IS_BINARY(args[2])) {

        LOG_ARGS(ARG_COUNT(command), args, "Execute SQL statement");

		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
													? ERL_INT_VALUE(args[1])
													: ERL_LL_UVALUE(args[1]));
		try {
	        ocistmt * statement_handle = conn_handle->prepare_stmt(ERL_BIN_PTR(args[2]), ERL_BIN_SIZE(args[2]));
			ETERM *stmth = erl_mk_ulonglong((unsigned long long)statement_handle);
			resp = erl_format((char*)"{~w,~i,{stmt,~w}}", args[0], PREP_STMT, stmth);
			erl_free_term(stmth);
		} catch (intf_ret r) {
			resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], PREP_STMT, r.gerrcode, r.gerrbuf);
			if (r.fn_ret == CONTINUE_WITH_ERROR) {
				if(!resp) REMOTE_LOG(INF, "Continue with ERROR Execute SQL \"%.*s;\" -> %s\n", ERL_BIN_SIZE(args[2]), ERL_BIN_PTR(args[2]), r.gerrbuf);
			} else {
				if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
				ret = true;
			}
		} catch (string str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], PREP_STMT, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], PREP_STMT);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(PREP_STMT), args);

	return ret;
}

bool command::bind_args(ETERM * command)
{
	bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(BIND_ARGS), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(BIND_ARGS)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], BIND_ARGS);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

    // Args: Connection Handle, Statement Handle, BindList
    if((ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1]))
		&& (ERL_IS_INTEGER(args[2]) || ERL_IS_UNSIGNED_LONGLONG(args[2]) || ERL_IS_LONGLONG(args[2]))
		&& ERL_IS_LIST(args[3])) {

		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
													? ERL_INT_VALUE(args[1])
													: ERL_LL_UVALUE(args[1]));

		ocistmt * statement_handle = (ocistmt*)(ERL_IS_INTEGER(args[2])
												? ERL_INT_VALUE(args[2])
												: ERL_LL_UVALUE(args[2]));

		try {
			if (!conn_handle->has_statement(statement_handle)) {
				resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], CLSE_STMT, "invalid statement handle");
			} else {
				map_schema_to_bind_args(args[3], statement_handle->get_in_bind_args());
				resp = erl_format((char*)"{~w,~i,ok}", args[0], BIND_ARGS);
			}
		} catch (intf_ret r) {
			resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], BIND_ARGS, r.gerrcode, r.gerrbuf);
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
			ret = true;
		} catch (string & str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], BIND_ARGS, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], BIND_ARGS);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
	if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(BIND_ARGS), args);

    return ret;
}

bool command::exec_stmt(ETERM * command)
{
	bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(EXEC_STMT), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(EXEC_STMT)){
	    resp = erl_format((char*)"{~w,~i,{error,badargcount,~i,~i}}", args[0], EXEC_STMT, ARG_COUNT(command), CMD_ARGS_COUNT(EXEC_STMT));
		if(!resp) REMOTE_LOG(ERR, "ERROR bad arguments count\n");
		ret = true;
	    goto error_exit;
	}

    // Args: Connection Handle, Statement Handle, Bind List, auto_commit
    if((ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1]))
		&& (ERL_IS_INTEGER(args[2]) || ERL_IS_UNSIGNED_LONGLONG(args[2]) || ERL_IS_LONGLONG(args[2]))
		&& ERL_IS_LIST(args[3])
		&& (ERL_IS_INTEGER(args[4]) || ERL_IS_UNSIGNED_LONGLONG(args[4]) || ERL_IS_LONGLONG(args[4]))) {
		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
													? ERL_INT_VALUE(args[1])
													: ERL_LL_UVALUE(args[1]));

		ocistmt * statement_handle = (ocistmt *)(ERL_IS_INTEGER(args[2])
													? ERL_INT_VALUE(args[2])
													: ERL_LL_UVALUE(args[2]));
		int auto_commit_val = (ERL_IS_INTEGER(args[4]) ? ERL_INT_VALUE(args[4]) : ERL_LL_UVALUE(args[4]));
		bool auto_commit = auto_commit_val > 0 ? true : false;
	    ETERM *columns = NULL, *rowids = NULL;
		try {
			if (!conn_handle->has_statement(statement_handle)) {
				resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], CLSE_STMT, "invalid statement handle");
				if(!resp) REMOTE_LOG(ERR, "ERROR invalid statement handle\n");
			} else {
				size_t bound_count = map_value_to_bind_args(args[3], statement_handle->get_in_bind_args());
				unsigned int exec_ret = statement_handle->execute(&columns, append_coldef_to_list, &rowids, append_string_to_list, auto_commit);
				if (bound_count) REMOTE_LOG(DBG, "Bounds %u", bound_count);
				// TODO : Also return bound values from here
				if (columns == NULL && rowids == NULL)
					resp = erl_format((char*)"{~w,~i,{executed,~i}}", args[0], EXEC_STMT, exec_ret);
				else if (columns != NULL && rowids == NULL)
					resp = erl_format((char*)"{~w,~i,{cols,~w}}", args[0], EXEC_STMT, columns);
				else if (columns == NULL && rowids != NULL)
					resp = erl_format((char*)"{~w,~i,{rowids, ~w}}", args[0], EXEC_STMT, rowids);
				else {
					resp = erl_format((char*)"{~w,~i,{cols,~w},{rowids, ~w}}", args[0], EXEC_STMT, columns, rowids);
				}
				if (rowids != NULL) erl_free_term(rowids);
				if (columns != NULL) erl_free_term(columns);
			}
		} catch (intf_ret r) {
			resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], EXEC_STMT, r.gerrcode, r.gerrbuf);
			if (r.fn_ret == CONTINUE_WITH_ERROR) {
				if(!resp) REMOTE_LOG(INF, "Continue with ERROR Execute STMT %s\n", r.gerrbuf);
			} else {
				if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
				ret = true;
			}
		} catch (string str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], EXEC_STMT, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], EXEC_STMT);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
	} else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(EXEC_STMT), args);

	return ret;
}

bool command::fetch_rows(ETERM * command)
{
	bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(FTCH_ROWS), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(FTCH_ROWS)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], FTCH_ROWS);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

    // Args: Connection Handle, Statement Handle, Rowcount
    if((ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1]))
		&& (ERL_IS_INTEGER(args[2]) || ERL_IS_UNSIGNED_LONGLONG(args[2]) || ERL_IS_LONGLONG(args[2]))
		&& ERL_IS_INTEGER(args[3])) {

		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
										? ERL_INT_VALUE(args[1])
										: ERL_LL_UVALUE(args[1]));

		ocistmt * statement_handle = (ocistmt*)(ERL_IS_INTEGER(args[2])
												? ERL_INT_VALUE(args[2])
												: ERL_LL_UVALUE(args[2]));
        int rowcount = ERL_INT_VALUE(args[3]);

		ETERM *rows = NULL;
		try {
			if (!conn_handle->has_statement(statement_handle)) {
				resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], CLSE_STMT, "invalid statement handle");
				if(!resp) REMOTE_LOG(ERR, "ERROR invalid statement handle\n");
			} else {
				intf_ret r = statement_handle->rows(&rows,
												   append_string_to_list,
												   append_list_to_list,
												   calculate_resp_size,
												   rowcount);
				if (r.fn_ret == MORE || r.fn_ret == DONE) {
					if (rows != NULL) {
						resp = erl_format((char*)"{~w,~i,{{rows,~w},~a}}", args[0], FTCH_ROWS, rows, (r.fn_ret == MORE ? "false" : "true"));
						erl_free_term(rows);
					}
					else
						resp = erl_format((char*)"{~w,~i,{{rows,[]},true}}", args[0], FTCH_ROWS);
				}
			}
		} catch (intf_ret r) {
			resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], FTCH_ROWS, r.gerrcode, r.gerrbuf);
			if (r.fn_ret == CONTINUE_WITH_ERROR) {
				if(!resp) REMOTE_LOG(INF, "Continue with ERROR fetch STMT %s\n", r.gerrbuf);
			} else {
				if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
				ret = true;
			}
		} catch (string str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], FTCH_ROWS, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], FTCH_ROWS);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
	if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(FTCH_ROWS), args);

    return ret;
}

bool command::close_stmt(ETERM * command)
{
	bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(CLSE_STMT), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(CLSE_STMT)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], CLSE_STMT);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

    // Args: Connection Handle, Statement Handle
    if((ERL_IS_INTEGER(args[1]) || ERL_IS_UNSIGNED_LONGLONG(args[1]) || ERL_IS_LONGLONG(args[1]))
		&& (ERL_IS_INTEGER(args[2]) || ERL_IS_UNSIGNED_LONGLONG(args[2]) || ERL_IS_LONGLONG(args[2]))) {

		ocisession * conn_handle = (ocisession *)(ERL_IS_INTEGER(args[1])
										? ERL_INT_VALUE(args[1])
										: ERL_LL_UVALUE(args[1]));

		ocistmt * statement_handle = (ocistmt*)(ERL_IS_INTEGER(args[2])
												? ERL_INT_VALUE(args[2])
												: ERL_LL_UVALUE(args[2]));
		try {
			if (!conn_handle->has_statement(statement_handle)) {
				resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], CLSE_STMT, "invalid statement handle");
				if(!resp) REMOTE_LOG(ERR, "ERROR invalid statement handle\n");
			} else {
				statement_handle->close();
				resp = erl_format((char*)"{~w,~i,ok}", args[0], CLSE_STMT);
			}
		} catch (intf_ret r) {
			resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], CLSE_STMT, r.gerrcode, r.gerrbuf);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
		} catch (string str) {
			resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], CLSE_STMT, str.c_str());
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
		} catch (...) {
			resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], CLSE_STMT);
			ret = true;
			if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
		}
    } else {
		REMOTE_LOG_TERM(ERR, command, "argument type(s) missmatch\n");
	}

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(CLSE_STMT), args);

    return ret;
}

bool command::echo(ETERM * command)
{
	bool ret = false;
    ETERM * resp = NULL;

    ETERM **args;
    MAP_ARGS(CMD_ARGS_COUNT(CMD_ECHOT), command, args);

    if(ARG_COUNT(command) != CMD_ARGS_COUNT(CMD_ECHOT)) {
	    resp = erl_format((char*)"{~w,~i,{error,badarg}}", args[0], CMD_ECHOT);
		if(!resp) REMOTE_LOG(ERR, "ERROR badarg\n");
		ret = true;
	    goto error_exit;
	}

    // Args: Erlang Term
	try {
		resp = erl_format((char*)"{~w,~i,~w}", args[0], CMD_ECHOT, args[1]);
	} catch (intf_ret r) {
		resp = erl_format((char*)"{~w,~i,{error,{~i,~s}}}", args[0], CMD_ECHOT, r.gerrcode, r.gerrbuf);
		ret = true;
		if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", r.gerrbuf);
	} catch (string str) {
		resp = erl_format((char*)"{~w,~i,{error,{0,~s}}}", args[0], CMD_ECHOT, str.c_str());
		ret = true;
		if(!resp) REMOTE_LOG(ERR, "ERROR %s\n", str.c_str());
	} catch (...) {
		resp = erl_format((char*)"{~w,~i,{error,{0,unknown}}}", args[0], CMD_ECHOT);
		ret = true;
		if(!resp) REMOTE_LOG(ERR, "ERROR unknown\n");
	}
    

error_exit:
	if(!resp) REMOTE_LOG(CRT, "driver error: no resp generated, shutting down port\n");
    if(write_resp(resp) < 0)
        ret = true;

	UNMAP_ARGS(CMD_ARGS_COUNT(CMD_ECHOT), args);

    return ret;
}

//#define PRINTCMD

bool command::process(void * param)
{
	bool ret = false;
	ETERM *command = (ETERM *)param;

	//PRINT_ERL_ALLOC("start");

	ETERM *cmd = erl_element(2, (ETERM *)command);
#ifdef PRINTCMD
	char * tmpbuf = print_term(command);
	REMOTE_LOG("========================================\nCOMMAND : %s %s\n========================================\n",
		CMD_NAME_STR(ERL_INT_VALUE(cmd)),
		tmpbuf);
	delete tmpbuf;
#endif

	if(ERL_IS_INTEGER(cmd)) {
        switch(ERL_INT_VALUE(cmd)) {
        case GET_SESSN:	ret = get_session(command);		break;
        case PUT_SESSN:	ret = release_conn(command);	break;
        case PREP_STMT:	ret = prep_sql(command);		break;
        case BIND_ARGS:	ret = bind_args(command);		break;
        case EXEC_STMT:	ret = exec_stmt(command);		break;
        case FTCH_ROWS:	ret = fetch_rows(command);		break;
        case CLSE_STMT:	ret = close_stmt(command);		break;
        case CMT_SESSN:	ret = commit(command);			break;
        case RBK_SESSN:	ret = rollback(command);		break;
        case RMOTE_MSG:	ret = change_log_flag(command);	break;
        case CMD_DSCRB:	ret = describe(command);		break;
        case CMD_ECHOT:	ret = echo(command);			break;
        case OCIP_QUIT:
        default:
			ret = true;
            break;
        }
    }

//	PRINT_ERL_ALLOC("end");

	return ret;
}

//#define PRINTCMD

bool command::process(void * param, term & t)
{
	bool ret = false;
	ETERM *command = (ETERM *)param;

	//PRINT_ERL_ALLOC("start");

	ETERM *cmd = erl_element(2, (ETERM *)command);
#ifdef PRINTCMD
	char * tmpbuf = print_term(command);
	REMOTE_LOG(DBG, "========================================\nCOMMAND : %s %s\n========================================\n",
		CMD_NAME_STR(ERL_INT_VALUE(cmd)),
		tmpbuf);
	delete tmpbuf;
#endif

	if(t.is_tuple() && t.lt[1].is_integer()) {
		switch(t.lt[1].v.i) {
        //case RMOTE_MSG:	ret = change_log_flag(command);	break;
        case RMOTE_MSG:	ret = change_log_flag(t);		break;
        case GET_SESSN:	ret = get_session(command);		break;
        case PUT_SESSN:	ret = release_conn(command);	break;
        case PREP_STMT:	ret = prep_sql(command);		break;
        case BIND_ARGS:	ret = bind_args(command);		break;
        case EXEC_STMT:	ret = exec_stmt(command);		break;
        case FTCH_ROWS:	ret = fetch_rows(command);		break;
        case CLSE_STMT:	ret = close_stmt(command);		break;
        case CMT_SESSN:	ret = commit(command);			break;
        case RBK_SESSN:	ret = rollback(command);		break;
        case CMD_DSCRB:	ret = describe(command);		break;
        case CMD_ECHOT:	ret = echo(command);			break;
        case OCIP_QUIT:
        default:
			ret = true;
            break;
        }
    }

//	PRINT_ERL_ALLOC("end");

	return ret;
}