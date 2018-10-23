#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <json-c/json.h>

pthread_t thread;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

//shared resource
json_object *users_list;
int end_run;

//print an error and end program
void error (char *msg){
		puts(msg);
		exit(0);
	}

void* receive(void* d){
		int sockfd=*((int *)d);
		int a;
		char buf[1024];
		char file_buffer[1000];
		int flag = 0; //check for the first data
		FILE *fp_thread;
		json_object *server_answer; //json response server
		json_object *status_server; //json key value
		json_object *jdata_server; //stores jdata
		json_object *temp_users; //temp user list
		json_object *temp_key_value; //stores value of x key
		json_object *registered_id; //stored id
		json_object *server_id; //server send id
		json_object *juser_object; //stores all user info
		json_object *in_array_data;
		char id_users[50];
		char id_server[50];
		char new_status[50];
		int i;		
	/*
	 *first server response
	 *response server: {"status": "OK", "user" : {"id":"1234","name":"xx","status":"active"}} 
	 */
	while(flag == 0){
		puts("waiting for server response...");
		bzero(buf, 1024);
		a = recv(sockfd, buf, 1024, 0);
		//error
		if( a<= 0){
			puts("Error reading from socket, no server response. Wait...");
			//close(sockfd);
			//pthread_exit(NULL);
		}
		
		//save json
		else{
			//convert answer
			server_answer = json_tokener_parse(buf);
			json_object_object_get_ex(server_answer, "status", &status_server);
			if(strcmp(json_object_to_json_string(status_server),"OK") == 0){
				puts("connected to server");
				flag = 1; //set first data received
				
				//get user list
				pthread_mutex_lock(&mutex);
				fp_thread = fopen("user.txt", "r");
				fgets(file_buffer,1000,fp_thread);
				fclose(fp_thread);
				pthread_mutex_unlock(&mutex);
				
				//string to json {file: users:[]}
				temp_users = json_tokener_parse(file_buffer);
				//get key array
				json_object_object_get_ex(temp_users, "users", &temp_key_value);
				//add user to array
				json_object_array_add(temp_key_value, json_object_object_get(server_answer, "user"));
				//convert json to array
				strncpy(file_buffer, json_object_to_json_string(temp_users),sizeof(file_buffer));
				
				//save array
				pthread_mutex_lock(&mutex);
				fp_thread = fopen("user.txt", "w+");
				fprintf(fp_thread, file_buffer);
				fclose(fp_thread);
				pthread_mutex_unlock(&mutex);
			}
			else{
				//print error message
				puts("error from server");
				json_object_object_get_ex(server_answer, "message", &status_server);
				puts(json_object_to_json_string(status_server));
			}
			
		}
	}
	
	//user is connected
	while(1){
		//receive message from server
		//server_answer & status_server & temp_key_value & temp_users[variables]
		int end_thread = 0;
		char json_string[100];
		bzero(buf,1024);
		a=recv(sockfd,buf,1024,0);
		server_answer = json_tokener_parse(buf); //create json object
		
		//take action of message
		//check if message was to end end_run
		pthread_mutex_lock(&mutex);
		end_thread = end_run;
		pthread_mutex_unlock(&mutex);
		if(end_thread == 1){
			close(sockfd);
			pthread_exit(NULL);
		}
		
		//get the key action || get the key status
		if(json_object_object_get_ex(server_answer, "action", &status_server)){
			//convert json to string
			strncpy(json_string,json_object_to_json_string(status_server),100);
			
			//get users list
			pthread_mutex_lock(&mutex);
			fp_thread = fopen("user.txt", "r");
			fgets(file_buffer, 1000, fp_thread);
			fclose(fp_thread);
			pthread_mutex_unlock(&mutex);
			//create json array
			temp_users = json_tokener_parse(file_buffer);
			json_object_object_get_ex(temp_users, "users", &temp_key_value);
			
			/* JSON : USER_CONNECTED ; temp_key_value: json array
			* action : USER_CONNECTED
			* user: {id:1234,name:asf,status:active}
			*/
			if(strcmp(json_string, "USER_CONNECTED") == 0){
				//get user in jdata_server : <id> <name> <status>
				json_object_object_get_ex(server_answer, "user", &jdata_server);
				//store <user>jdata_server -> <array>temp_key_value
				json_object_array_add(temp_key_value, jdata_server);
				//save string
				strncpy(file_buffer, json_object_to_json_string(temp_users),sizeof(file_buffer));
				pthread_mutex_lock(&mutex);
				fp_thread = fopen("user.txt", "w+");
				fprintf(fp_thread, file_buffer);
				fclose(fp_thread);
				pthread_mutex_unlock(&mutex);
			}
			
			/* JSON : USER_DISCONNECTED ; temp_key_value: json array
			* action : USER_DISCONNECTED
			* user: {id:1234,name:asf,status:active}
			*/
			else if(strcmp(json_string, "USER_DISCONNECTED") == 0){
				//string storage
				//char id_users[50];
				//char id_server[50];
				
				//get user in jdata_server: <id><name><status>
				json_object_object_get_ex(server_answer, "user", &jdata_server);
				json_object_object_get_ex(jdata_server, "id", &server_id);
				
				strncpy(id_server, json_object_to_json_string(server_id),sizeof(id_server));
				
				//search for the user in the system array
				for(i=0; i<json_object_array_length(temp_key_value); i++){
					juser_object = json_object_array_get_idx(temp_key_value, i);
					json_object_object_get_ex(juser_object, "id", &in_array_data);
					strncpy(id_users, json_object_to_json_string(in_array_data),sizeof(id_users));
					
					if(strcmp(id_server, id_users) == 0){
						//id matched -> remove from list
						json_object_array_del_idx(temp_key_value, i, 1);
						//create new json string & save
						strncpy(file_buffer, json_object_to_json_string(temp_users),sizeof(file_buffer));
						pthread_mutex_lock(&mutex); //saving
						fp_thread = fopen("user.txt", "w+");
						fprintf(fp_thread, file_buffer);
						fclose(fp_thread);
						pthread_mutex_unlock(&mutex);
						break;
					}
				}
			}
			
			/* JSON : RECEIVE_MESSAGE ; temp_key_value: json array
			* action : RECEIVE_MESSAGE
			* from : 1234
			* to : 9876
			* message : "holis"
			*/
			else if(strcmp(json_string, "RECEIVE_MESSAGE") == 0){
				//get from key in server_answer to jdata_server
				json_object_object_get_ex(server_answer, "from", &jdata_server);
				//id_server in string
				strncpy(id_server, json_object_to_json_string(jdata_server),sizeof(id_server));
				
				//search for the user in the array
				for(i=0; i<json_object_array_length(temp_key_value); i++){
					//get juser in array[x]
					juser_object = json_object_array_get_idx(temp_key_value, i);
					json_object_object_get_ex(juser_object, "id", &in_array_data);
					strncpy(id_users, json_object_to_json_string(in_array_data),sizeof(id_users));
					
					//compare ids
					if(strcmp(id_server, id_users) == 0){
						//get the name of sender
						json_object_object_get_ex(juser_object, "name", &in_array_data);
						strncpy(file_buffer, json_object_to_json_string(in_array_data), sizeof(file_buffer));
						
						//print message
						puts("message from: ");
						puts(file_buffer);
						puts("message: ");
						json_object_object_get_ex(server_answer, "message",&in_array_data);
						strncpy(file_buffer, json_object_to_json_string(in_array_data),sizeof(file_buffer));
						puts(file_buffer);
						break;
					}
				}
			}
			
			/* JSON : LIST_USER [x](always an array)
			* action : LIST_USER
			* users : [{id,name,status},{id,name,status}]
			*/
			else if(strcmp(json_string, "LIST_USER") == 0){
				//get users list from server jdata_server:[array]
				json_object_object_get_ex(server_answer,"users",&jdata_server);
				//display every object in the array
				for(i=0; i<json_object_array_length(jdata_server); i++){
					//get name
					//get the object with all info
					juser_object = json_object_array_get_idx(jdata_server,i);
					//get name & print
					json_object_object_get_ex(juser_object, "name", &in_array_data);
					strncpy(file_buffer,json_object_to_json_string(in_array_data),sizeof(file_buffer));
					
					json_object_object_get_ex(juser_object, "status", &in_array_data);
					strncpy(id_users, json_object_to_json_string(in_array_data), sizeof(id_users));
					//create string
					strcat(file_buffer,id_users);
					
					puts("Name		|		status");
					puts(file_buffer);
				}
			}
			
			/* JSON : CHANGED_STATUS ; temp_key_value: json array
			* action : CHANGED_STATUS
			* user : {id,name,status}
			*/
			else if(strcmp(json_string, "CHANGED_STATUS") == 0){
				//get id from server
				json_object_object_get_ex(server_answer,"user",&jdata_server);
				json_object_object_get_ex(jdata_server, "id", &server_id);
				//convert to string
				strncpy(id_server, json_object_to_json_string(server_id), sizeof(id_server));
				
				//get the new status 
				json_object_object_get_ex(jdata_server, "status", &server_id); //get the new status object
				strncpy(new_status, json_object_to_json_string(server_id), sizeof(new_status)); //to prevent pointer reference
				
				//search for user in system array
				for(i=0;i<json_object_array_length(temp_key_value);i++){
					//get user in array
					juser_object = json_object_array_get_idx(temp_key_value, i);
					json_object_object_get_ex(juser_object, "id", &in_array_data);
					
					strncpy(id_users, json_object_to_json_string(in_array_data), sizeof(id_users)); //save string
					
					//compare id_users-id_server
					if(strcmp(id_users,id_server) == 0){
						json_object_object_get_ex(temp_key_value,"status", &in_array_data); //get status pointer
						in_array_data = json_object_new_string(new_status); //change status
						strncpy(file_buffer, json_object_to_json_string(temp_users), sizeof(file_buffer)); //create new string
						pthread_mutex_lock(&mutex); //saving in file
						fp_thread = fopen("user.txt", "w+");
						fprintf(fp_thread, file_buffer);
						fclose(fp_thread);
						pthread_mutex_unlock(&mutex);
						break;						
					}
					
				}
			}
		}
		else{
			/* JSON : ERROR
			* status : ERROR
			* message : "error"
			*/
			puts("SERVER ERROR");
			json_object_object_get_ex(server_answer, "message", &status_server);
			puts(json_object_to_json_string(status_server));
			
		}
	}
}


int main(int argc, char *argv[]){
	int sockfd, portno, n, newsockfd,userno;
	char *username, *clientPort, *serverIP, *clientIP;
	struct sockaddr_in arr_in, serv_addr;
	struct hostent *server;
	char buffer[256];
	char * status_change;
	char * msg_buffer;
	char * ask_user;
	json_object *action_ob;
	end_run = 0; //tells the thread to kill himself [0:false]
	FILE *fp;
	
	//check arguments
	if (argc<5){
		error("ERROR : client <user> <server_port> <server_ip> <client_ip>");
	}
	
	//get data
	username = argv[1];
	portno=atoi(argv[2]);
	serverIP = argv[3];
	clientIP = argv[4];
	
	//create port
	sockfd=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd<0){
		error("ERROR: failed opening port");
	}
	//give server ip
	server=gethostbyname(argv[3]);
	if (server==NULL) {
		error("ERROR: Host not found");
	}
    
    bzero((char*)&serv_addr, sizeof(serv_addr));
     
    serv_addr.sin_family=AF_INET;
    bcopy((char*)server->h_addr, (char*)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port=htons(portno);
    newsockfd = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
	
    if (newsockfd<0){
		error("ERROR: failed conection");
	}
	else{
		//create json object
		action_ob = json_object_new_object();
		//create json values
		json_object * hostj = json_object_new_string(serverIP);
		json_object * originj = json_object_new_string(clientIP);
		json_object * userj = json_object_new_string(username);
		//create key:value
		json_object_object_add(action_ob, "host", hostj);
		json_object_object_add(action_ob, "origin", originj);
		json_object_object_add(action_ob, "user", userj);
		//create string
		bzero(buffer,256);
		char connect_req[255];
		strncpy(connect_req, json_object_to_json_string(action_ob), 255);
		char a1[50];
		
		//create file
		strncpy(buffer, "{\"users\" : []}", 256);
		fp = fopen("users.txt", "w");
		fprintf(fp, buffer);
		fclose(fp);
		//create thread
		
		pthread_create(&thread,NULL,receive,(void*)&sockfd);
		
		//check correct send
		if((send(sockfd,connect_req,255,0))<0){
			printf("ERROR ENVIANDO MENSAJE A SERVIDOR");
		}
		
		//menu
		while(1){
		
			printf("\n%s\n","-----------*   MENU   *-------------");
			printf("\n%s\n","1. Cambiar de status");
			printf("\n%s\n","2. Obtener informacion de un usuario");
			printf("\n%s\n","3. Obtener el listado de usuarios");
			printf("\n%s\n","4. Enviar mensaje a un usuario");
			printf("\n%s\n","5. Enviar mensaje a todos");
			printf("\n%s\n","6. Cerrar conexion y salir");
			scanf("%s", &a1);
		
			//change status
			if (strcmp(a1,"1")==0){				//status change
				char a2[50];
				printf("Nuevo status?:\n");
				printf("0. Activo\n");
				printf("1. Idle\n");
				printf("2. Away\n");
				scanf("%s",&a2);
				if(strcmp(a2,"0")==0||strcmp(a2,"1")==0||strcmp(a2,"2")==0){
					/* 
					bzero(buffer,256);
					snprintf(buffer,sizeof(buffer),"%s",username);
					send(sockfd,buffer,255,0);
					*/
					if(strcmp(a2,"0")==0){
						//*agregar id en user
						status_change = "{\"action\" : \"CHANGE_STATUS\", \"user\" : \"chris\", \"status\" : \"active\"}";
						send(sockfd, status_change, 255, 0);
						puts("Nuevo estado: Activo\n");
						}
					else if(strcmp(a2,"1")==0){
						//*agregar id en user
						status_change = "{\"action\" : \"CHANGE_STATUS\", \"user\" : \"chris\", \"status\" : \"busy\"}";
						send(sockfd, status_change, 255, 0);
						printf("Tu estado ahora es: busy\n");
						}
					else if(strcmp(a2,"2")==0){
						//*agregar id en user
						status_change = "{\"action\" : \"CHANGE_STATUS\", \"user\" : \"chris\", \"status\" : \"inactive\"}";
						send(sockfd, status_change, 255, 0);
						printf("Tu estado ahora es: inactive\n");
						}
					
				}
				else{
					printf("ERROR: estado incorrecto\n");
					}
				}
			
			//user info
			else if (strcmp(a1,"2")==0){
				char a3[50], status[50];
				char uinfobuff[256];
				
				printf("Ingrese el nombre del usuario\n");
				scanf("\n%s", &a3);
				//create request
				//create json string
				json_object * jask_user = json_object_new_object();
				//json objects
				action_ob = json_object_new_string("LIST_USER");
				json_object *user_ob = json_object_new_string(a3);
				//add objects
				json_object_object_add(jask_user, "action", action_ob);
				json_object_object_add(jask_user, "user", user_ob);
				//get string
				strncpy(buffer, json_object_to_json_string(jask_user), sizeof(buffer));
				send(sockfd,buffer,255,0);
				}
				
			//user list	
			else if (strcmp(a1,"3")==0){
				//create request
				ask_user = "{\"action\" : \"LIST_USER\"}";
				send(sockfd, ask_user, 255, 0);				
				}
			
			//send msg
			else if (strcmp(a1,"4")==0){
				char a4[50], a5[256];
				
				//GET DATA
				printf("TO:\n");
				scanf("%s", &a4);
				
				printf("MSG[255]: \n");
				scanf("%s", &a5);
				
				//SEND MSG
				//create json string
				json_object * jmsg = json_object_new_object();
				//json objects
				action_ob = json_object_new_string("SEND_MESSAGE");
				json_object *from_ob = json_object_new_string("chris");
				json_object *to_ob = json_object_new_string(a4);
				json_object *message_ob = json_object_new_string(a5);
				//add objects
				json_object_object_add(jmsg, "action", action_ob);
				json_object_object_add(jmsg, "from", from_ob);
				json_object_object_add(jmsg, "to", to_ob);
				json_object_object_add(jmsg, "message", message_ob);
				//get string
				strncpy(buffer, json_object_to_json_string(jmsg), sizeof(buffer));
				//send
				send(sockfd,buffer,255,0);
				}
			
			//send to ALL
			else if (strcmp(a1,"5")==0){
				printf("POR IMPLEMENTAR");
			}

			//exit
			else if (strcmp(a1,"6")==0){
				bzero(buffer,256);
				snprintf(buffer,sizeof(buffer),"BYE"); //log out
				send(sockfd,buffer,255,0);
				puts("Cerrando programa...");
				pthread_mutex_lock(&mutex);
				end_run = 1; //[1:true -> kill]
				pthread_mutex_unlock(&mutex);
				break;
				}
				
			//invalid option
			else{
				printf("ERROR: opcion incorrecta");
				}
		}		
	}
	
}
