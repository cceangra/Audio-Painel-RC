// Lançador ELF pequeno, para duplo clique. Não muda o diretório do usuário.
// Sem C++/GTK/Python adicionais: entrega o controle ao executável da pasta.
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
int main(int argc,char** argv){
    char path[PATH_MAX];ssize_t length=readlink("/proc/self/exe",path,sizeof path-1);if(length<0)return 1;path[length]=0;char* slash=strrchr(path,'/');if(!slash)return 1;
    if((size_t)(slash-path)+sizeof "/audio-painel-rc">sizeof path)return 1;strcpy(slash,"/audio-painel-rc");
    if(argc==1){char* args[]={path,"--iniciar",NULL};execv(path,args);}else{argv[0]=path;execv(path,argv);}
    fprintf(stderr,"Não foi possível abrir o Áudio RC. Confira o executável audio-painel-rc nesta pasta.\n");return 1;
}
