//特别感谢这位大佬的讲解：https://blog.csdn.net/qq_66026801/article/details/130520032
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <sstream>
#include <cstring>
using namespace std;

//引导扇区的前32字节，62字节后是代码区，最后两字节是0xaa55
//事实上因为本实验就是固定的这个a.img，这些属性是可以直接默认知道的（也就是可以不用读取它）
//由这些值可知，簇号==扇区号，扇区0：引导扇区，扇区1-9：FAT1，扇区10-18：FAT2；扇区19-32：根目录区；扇区33开始：其它数据区
struct BootRecord{
    char jmp[3];
    char OEMidentifier[8];
    char bytesPerSector[2];  //每扇区字节数，默认512
    char sectorsPerCluster;  //每簇的扇区数，默认1
    char reservedSectors[2]; //引导+保留的扇区数，默认1
    char numberOfFAT;        //FAT表数，默认2
    char numberOfDirectoryEntry[2]; //根目录文件数的最大值，乘以32就是给根目录区预留的字节数；再除以512就是根目录区占的扇区数，默认14。
    char numberOfSertors[2]; //扇区数，为2880，也就是1.44M
    char mediaDescriptorType;
    char sectorsPerFAT[2];   //一个FAT表的扇区数，默认为9
    char otherAttributions[8]; //保留，本实验用不到
};

//目前主流机器都是小端存储，这里的short和int是用小端解析的，所以能正确解析
//存取时会根据size来进行对齐，即如果short的地址是25、26的话，会对齐成26、27，就不能正确解析了
//因此，如果定义结构体时希望直接解析一个整数，需要看好它的起始地址是否能和该数据类型对齐！！！
struct DirectoryEntry {
    char fileName[8];            //文件名
    char extensionName[3];       //扩展名
    char attribution;            //文件属性，详见PPT
    char otherAttributions[14];  //保留，本实验用不到
    short fileStartCluster;  //起始簇号
    int fileSize;       //文件大小
};

struct File {
    string fileName;
    string path;
    char attribution;           
    short fileStartCluster;       
    int size;
    void init(string fileName, string path, char attribution, short fileStartCluster, int size){
        this->fileName = fileName;
        this->path = path;
        this->attribution = attribution;
        this->fileStartCluster = fileStartCluster;
        this->size = size;
    }
};

struct Instruction{
    string command;
    string option;
    string path;
    void reset(){
        command = "";
        option = "";
        path = "";
    }
};

const char imgName[] = "a.img";
bool hasError; //是否有错误
string errorMessage; //错误信息

BootRecord bootRecord;  
char FAT1[9*512];
DirectoryEntry rootDirectoryEntry[224]; 
Instruction instruction;
std::ifstream img;
char fileTemp[2000005];

void readBasicInfomation();    //读取引导扇区前32字节、FAT表、224个根目录项
void solveInstruction();       //输入并处理指令。
void readInstruction();        //读取并初步解析命令，存储到全局变量instruction中。如有明显错误，则设置hasError和errorMessage。
void solveLs(File f);           //处理ls指令
void solveCat();                //处理cat指令
File findPath(string path);     //根据路径，返回File（根目录起始簇号返回0，不存在返回-1）
int getNextCluster(int cluster);//根据当前簇号获取下一簇号，-1表示结束，-2表示异常。
void redPrint(const char *s);
void whitePrint(const char *s);
void split(string str, vector<string> &vec, char splitChar);  //实现字符串分割
void readDirectoryEntry(DirectoryEntry* dest, int startAddress); //读取目录项、并将名称里的空格替换成空。
void readSonDirectoryEntries(vector<File> &vec, File f); //初步读取所有子目录项、返回File列表。
int* countDirAndFile(vector<File> vec); //统计vec里的目录和文件数，不含.和..
void replaceChar(char str[], char source, char dest, int len);//将字符数组中的源字符替换为目标字符。
void setErrorMessage(string message); //设置出错位、出错信息。
string joinFileName(char srcName[], char srcExtension[]); //联合全名和扩展名
int min(int a, int b);
extern "C" void myPrint(const char *str);

int main(){
    img.open(imgName);
    if(!img.is_open()){
        whitePrint("Image Not Found!");
        return 0;
    }
    readBasicInfomation();
    while (true) {
        solveInstruction();
        if(hasError){
            whitePrint((errorMessage+"\n").c_str());
        }else if(instruction.command == "exit"){
            break;
        }
    }
}

void readBasicInfomation(){
    BootRecord *bootPointer = &bootRecord;
    img.seekg(0);
    img.read((char*)bootPointer, 32); //用指针把这32字节整体读进去，然后再用结构体对各属性进行解析
    img.seekg(512);
    img.read(FAT1, 9*512);
    int start = (1+9+9) * 512;
    for(int i = 0; i < 224; i++){
        readDirectoryEntry(&rootDirectoryEntry[i], start + i * 32);
    }
}

void solveInstruction(){
    hasError = false;
    whitePrint(">");
    readInstruction();
    if(hasError) return;
    if(instruction.command == "ls"){
        solveLs(findPath(instruction.path));
    }else if(instruction.command == "cat"){
        solveCat();
    }
}

void readInstruction(){//这里不检验path是否存在。
    string input;
    instruction.reset();
    getline(cin, input);
    vector<string> vec;
    split(input, vec, ' ');
    if(vec[0] != "cat" && vec[0] != "ls" && vec[0] != "exit"){
        setErrorMessage("Error: Command Type Does Not Exist!");
        return;
    }
    instruction.command = vec[0];
    for(int i = 1; i < vec.size(); i++){
        if(vec[i][0] == '-'){  //处理option。由于option可以多次重复，因此这里选择把所有的都拼起来，最后看是不是全是l。
            instruction.option = instruction.option + vec[i].substr(1);
        }else{  //处理path。
            if(instruction.path == ""){
                instruction.path = (vec[i][0] == '/' ? "" : "/") + vec[i];
            }else{ //如果path不为空，说明已经接受过路径了，因为不允许有多条路径，因此直接返回。
                setErrorMessage("Error: More Than One Path!");
                return;
            }
        }
    }
    if(instruction.command == "exit" && (instruction.option != "" || instruction.path != "")){
        setErrorMessage("Error: More Parameters Than Required!");
    }else if(instruction.command == "cat"){
        if(instruction.option != "") setErrorMessage("Error: More Parameters Than Required!");
        if(instruction.path == "") setErrorMessage("Error: Less Parameters Than Required!");
    }else{
        if(!regex_match(instruction.option, regex("^[l]*$"))) setErrorMessage("Error: Option Type Error!");
        if(instruction.path == "") instruction.path = "/";
    }
}

void solveLs(File f){
    if(f.fileStartCluster == -1){
        setErrorMessage("Error: Path Does Not Exist!");
        return;
    }
    if(f.attribution != 0x10){
        if(instruction.option == ""){
            whitePrint((f.fileName + "\n").c_str());
        }else{
            stringstream strTemp;
            strTemp << f.fileName << " " << f.size << "\n";
            whitePrint(strTemp.str().c_str());
        }
    }else{
        vector<File> vec;
        readSonDirectoryEntries(vec, f);
        if(hasError) return;
        if(instruction.option == ""){//分为是否有参数两种情况
            whitePrint((f.path + ":\n").c_str());
            for(int i = 0; i < vec.size(); i++){
                if(vec[i].attribution == 0x10){
                    redPrint(vec[i].fileName.c_str());
                }else{
                    whitePrint(vec[i].fileName.c_str());
                }
                whitePrint(i < vec.size() - 1 ? "  " : "\n");
            }
        }else{
            int* count = countDirAndFile(vec);
            stringstream strTemp;
            strTemp << f.path << " " << count[0] << " " << count[1] << ":\n";
            whitePrint(strTemp.str().c_str());
            strTemp.str("");
            for(int i = 0; i < vec.size(); i++){
                if(vec[i].attribution == 0x10){
                    redPrint(vec[i].fileName.c_str());
                    if(vec[i].fileName[0] != '.'){
                        vector<File> tempVec;
                        readSonDirectoryEntries(tempVec, vec[i]);
                        count = countDirAndFile(tempVec);
                        strTemp << " " << count[0] << " " << count[1];
                    }
                    whitePrint((strTemp.str() + "\n").c_str());
                    strTemp.str("");
                }else{
                    strTemp << vec[i].fileName << " " << vec[i].size << "\n";
                    whitePrint(strTemp.str().c_str());
                    strTemp.str("");
                }
            }
            whitePrint("\n");
        }
        for(int i = 0; i < vec.size(); i++){//递归处理子目录
            if(vec[i].attribution == 0x10 && vec[i].fileName[0] != '.') solveLs(vec[i]);
        }
    }
}

void solveCat(){
    File f = findPath(instruction.path);
    if(f.fileStartCluster == -1){
        setErrorMessage("Error: Path Does Not Exist!");
    }else if(f.attribution == 0x10){
        setErrorMessage("Error: This is a Directory!");
    }else{
        int cluster = f.fileStartCluster;
        memset(fileTemp, 0, sizeof(fileTemp)); 
        for(int i = 0; cluster >= 2; i++){
            img.seekg((cluster + 31) * 512);
            img.read((char*)(fileTemp + i * 512), 512);
            cluster = getNextCluster(cluster);
        }
        if(cluster != -1){
            setErrorMessage("Error: The File May Be Corrupted!");
            return;
        }
        whitePrint(strcat(fileTemp, "\n"));
    }
}

File findPath(string path){
    vector<string> vec;
    split(path, vec, '/');
    File ans;
    ans.fileStartCluster = -1;
    DirectoryEntry dirEntry; //当前目录项
    dirEntry.fileStartCluster = 0;
    int len = vec.size();
    for(int i = 0; i < len; i++){
        if(dirEntry.fileStartCluster == 0){//根目录的情况
            if(vec[i] == "." || vec[i] == "..") continue;
            for(int j = 0; j < 224; j++){//遍历根目录项
                if(rootDirectoryEntry[j].fileName[0] == '\0') break;
                string fullName = joinFileName(rootDirectoryEntry[j].fileName, rootDirectoryEntry[j].extensionName);
                if(vec[i] == fullName){//找到了，改变dirEntry，继续看下一个vec[i]
                    dirEntry = rootDirectoryEntry[j];
                    if(i < len-1 && dirEntry.attribution != 0x10) return ans; //路径的中间不应该有文件
                    break;
                }
            }
            if(dirEntry.fileStartCluster == 0) return ans;//cluster没改变，说明没找到，直接返回
        }else{//非根目录的情况            
            DirectoryEntry tempDirEntry;
            short tempCluster = dirEntry.fileStartCluster;
            while(true){
                int start = (31 + tempCluster) * 512;
                bool hasFind = false;
                for(int j = 0; j < 16; j++){
                    readDirectoryEntry(&tempDirEntry, start + j * 32);
                    string fullName = joinFileName(tempDirEntry.fileName, tempDirEntry.extensionName);
                    if(vec[i] == fullName){
                        dirEntry = tempDirEntry;
                        hasFind = true;
                        break;
                    }
                }
                if(hasFind){
                    if(i < len-1 && dirEntry.attribution != 0x10) return ans;
                    break;
                }else{
                    tempCluster = getNextCluster(tempCluster);
                    if(tempCluster < 0) return ans;
                }
            }    
        }
    }
    ans.path = "/";
    for(int i = 0; i < vec.size(); i++){
        if(vec[i] == "." || (vec[i] == ".." && ans.path == "/")){
            continue;
        }else if(vec[i] == ".."){
            ans.path = ans.path.substr(0, ans.path.size()-1);
            ans.path = ans.path.substr(0, ans.path.find_last_of("/")+1);
        }else{
            ans.path = ans.path + vec[i] + "/";
        }
    }
    if(vec.size() > 0){
        ans.init(vec[vec.size()-1], ans.path, dirEntry.attribution, dirEntry.fileStartCluster, dirEntry.fileSize);
    }else{
        ans.init("/", ans.path, 0x10, 0, 0);
    }
    return ans;
}

int getNextCluster(int cluster) {
    if (cluster<2) return -2;
    int index = cluster*1.5;  // 每一个簇在FAT表中占12bit；Double转int会向下取整
    int res = 0;
    unsigned short num1 = FAT1[index], num2 = FAT1[index+1];
    if (cluster % 2 == 0) {   
        num2 = num2 & 0x000f;   
        res = (num2 << 8) + num1; 
    } else {  
        num1 = num1 & 0x00f0;  
        res = (num1 >> 4) + (num2 << 4);
    }
    if (res >= 0x0ff8) return -1;  //最后一个簇
    if (res == 0x0ff7) return -2; //坏簇
    return res;
}

void redPrint(const char *s) {
    string str(s);
    myPrint("\033[31m");
    myPrint(str.c_str());
    myPrint("\033[37m");
}

void whitePrint(const char *s) {
    myPrint(s);
}

void split(string str, vector<string> &vec, char splitChar){
    int len = str.length();
    int l = 0, r = 0;
    while (true) {
        while(l < len && str[l] == splitChar) l++;
        if(l >= len) break;
        r = l + 1;
        while(r < len && str[r] != splitChar) r++;
        vec.push_back(str.substr(l, r-l));
        l = r + 1;
    }
}

void readDirectoryEntry(DirectoryEntry* dest, int startAddress){
    img.seekg(startAddress);
    img.read((char*)dest, 32);
    replaceChar(dest->fileName, ' ', '\0', 8); //为了方便，把名称中的空格替换成空白
    replaceChar(dest->extensionName, ' ', '\0', 3);
}

void readSonDirectoryEntries(vector<File> &vec, File f){
    DirectoryEntry tempDir;
    short cluster = f.fileStartCluster;
    bool isRoot = (cluster == 0);
    while (cluster >= 0 || isRoot) {
        for(int i = 0; i < (isRoot ? 224 : 16); i++){
            int start = (isRoot ? 19 : 31 + cluster) * 512;
            readDirectoryEntry(&tempDir, start + i * 32);
            if(tempDir.fileName[0] == '\0') break;
            File tempFile;
            tempFile.fileName = joinFileName(tempDir.fileName, tempDir.extensionName);
            tempFile.init(tempFile.fileName, (f.path + tempFile.fileName + "/"), tempDir.attribution,  tempDir.fileStartCluster, tempDir.fileSize);
            vec.push_back(tempFile);
        }
        if(isRoot) break;
        cluster = getNextCluster(cluster);
    }
    if(cluster < -1) setErrorMessage("Error: The Directory May Be Corrupted!");
}

int* countDirAndFile(vector<File> vec){
    int* ans = new int[2]; //需要动态分配内存
    memset(ans, 0, 2 * sizeof(int));
    for(int i = 0; i < vec.size(); i++){
        if(vec[i].attribution == 0x10){
            if(vec[i].fileName[0] != '.') ans[0]++;
        }else{
            ans[1]++; 
        }
    }
    return ans;
}

void replaceChar(char str[], char source, char dest, int len){
    for(int i = 0; i < len; i++){
        if(str[i] == source) str[i] = dest;
    }
}

void setErrorMessage(string message){
    hasError = true;
    errorMessage = message;
}

string joinFileName(char srcName[], char srcExtension[]){
    if(srcExtension[0] == '\0') return srcName;
    string temp(srcName);
    string tempExtension(srcExtension);
    return (temp.substr(0, min(temp.size(), 8)) + "." + tempExtension.substr(0, min(tempExtension.size(), 3)));
}

int min(int a, int b){
    return a < b ? a : b;
}