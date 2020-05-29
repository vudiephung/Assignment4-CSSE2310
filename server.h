#ifndef SERVER_H
#define SERVER_H

int set_up(const char* port);
// void handle_requests(struct Thread) {
//     while (conn_fd = accept(server, 0, 0), conn_fd >= 0) { // change 0, 0 to get info about other end
//         threadData->conn_fd = conn_fd;
//         threadData->mapData = mapData;
//         threadData->lock = &lock;
//         pthread_create(&threadId, 0, handle_request, (void*)threadData);
//     }  
// }

#endif