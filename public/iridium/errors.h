/// @file public/iridium/errors.h
/// @brief Return codes for system calls and many internal functions

#ifndef PUBLIC_IRIDIUM_ERRORS_H_
#define PUBLIC_IRIDIUM_ERRORS_H_

/// @brief Success
#define IR_OK (0)

/// @brief Unspecified or unknown error encountered
#define IR_ERROR_UNSPECIFIED (-1)

/// @brief The requested operation is not supported or implimented
#define IR_ERROR_UNSUPPORTED (-2)

/// @brief An invalid handle or one not owned by the current process was used
#define IR_ERROR_BAD_HANDLE (-3)

/// @brief The provided handle references the wrong type of object for the operation
#define IR_ERROR_WRONG_TYPE (-4)

/// @brief The system does not have enough free memory to perform the operation
#define IR_ERROR_NO_MEMORY (-5)

/// @brief The arguments passed to a function are not valid
#define IR_ERROR_INVALID_ARGUMENTS (-6)

/// @brief The provided memory buffer is not large enough to fit the request data
#define IR_ERROR_BUFFER_TOO_SMALL (-7)

/// @brief A reasource in the same location or by the same name already exists
#define IR_ERROR_ALREADY_EXISTS (-8)

/// @brief The requested resource does not exist
#define IR_ERROR_NOT_FOUND (-9)

/// @brief The object is not in an operable state, such as a destoryed `v_addr_region`
#define IR_ERROR_BAD_STATE (-10)

/// @brief The other side of a paired object has been deleted
#define IR_ERROR_PEER_CLOSED (-11)

/// @brief The process/handle does not have the correct rights to perform the operation
#define IR_ERROR_ACCESS_DENIED (-12)

/// @brief The operation did not complete before the deadline
#define IR_ERROR_TIMED_OUT (-13)

#endif // PUBLIC_IRIDIUM_ERRORS_H_
