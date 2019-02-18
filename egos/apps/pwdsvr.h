struct pwdsvr_request {
	unsigned int uname_size;			// size of user name
	unsigned int opwd_size;				// size of old password
	unsigned int npwd_size;				// size of new password
};

struct pwdsvr_reply {
	enum pwdsvr_status { PWDSVR_OK, PWDSVR_ERROR } status;
};

/* Interface to password service.
 */
bool_t pwdsvr_change(gpid_t svr, char *uname, char *opwd, char *npwd);
