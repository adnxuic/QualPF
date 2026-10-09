#include "gui/reference/libcomune.h"
#include "gui/reference/libmath.h"
#include <cstring>
#include <QMessageBox>
/************************************************************/
/*************** Chemical elements management ***************/
/************************************************************/

QColor get_elem_color(int num)
{
    if((num >=0) && (num < N_ELEMENTS))
        return JAV_ELEMENTY[num].color;
    else
        return QColor();
}

QColor get_amino_color(int num)
{
    if((num >=0) && (num < N_AMINO))
        return JAV_AMINO[num].color;
    else
        return QColor();
}

QColor get_chain_color(int num)
{
    if((num >=0) && (num < N_CHAINS))
        return JAV_CHAIN[num].color;
    else
        return QColor();
}

QColor get_struct_color(int num)
{
    if((num >=0) && (num < N_STRUCT))
        return JAV_STRUCTURE[num].color;
    else
        return QColor();
}

QColor get_base_color(int num)
{
    if((num >=0) && (num < N_DNA_NUCLEOTIDES))
        return JAV_DNA_NUCLEOTIDES[num].color;
    else
        return QColor();
}

QColor get_gen_color(int num)
{
    if(num < N_ELEMENTS)
       return get_elem_color(num);
    else if(num < (N_AMINO+N_ELEMENTS))
       return get_amino_color(num-N_ELEMENTS);
    else if(num < (N_ELEMENTS+N_AMINO+N_CHAINS))
       return get_chain_color(num-N_ELEMENTS-N_AMINO);
    else if(num < (N_ELEMENTS+N_AMINO+N_CHAINS+N_DNA_NUCLEOTIDES))
       return get_base_color(num-N_ELEMENTS-N_AMINO-N_CHAINS);
    else if(num < (N_ELEMENTS+N_AMINO+N_CHAINS+N_DNA_NUCLEOTIDES+N_STRUCT))
       return get_struct_color(num-N_ELEMENTS-N_AMINO-N_CHAINS-N_DNA_NUCLEOTIDES);
    else
    {
       QColor col = QColor();
       if(num == (N_ELEMENTS+N_AMINO+N_CHAINS+N_DNA_NUCLEOTIDES+N_STRUCT))
       {
          col.setRedF(0);
          col.setGreenF(0.8f);
          col.setBlueF(0.8f);
          col.setAlphaF(1);
       }
       return col;
    }
}

int ZFromPtab(int ptab)
{
    if((ptab >=0) && (ptab < N_ELEMENTS))
       return JAV_ELEMENTY[ptab].Z;
    return 0;
}

bool caseInsCompare(const std::string &s1, const std::string &s2)
{
    return((s1.size() == s2.size()) &&
            equal(s1.begin(), s1.end(), s2.begin(), caseInsCharCompareN));
}

bool is_ghost(std::string el)
{
    return caseInsCompare(trim(el), "Q");
}

int is_valid_symbol(std::string el, int Ntot)
{
    if(!Ntot)
       Ntot = N_ELEMENTS;
    for(int i=0; i<Ntot; i++)
    {
      if(caseInsCompare(trim(el), JAV_ELEMENTY[i].szSymbol))
        return i+1;
    }
    return 0;
}

int IsAmino(const std::string &resname)
{
    if(resname.empty())
       return 0;
    for(int i=0; i<N_AMINO; i++)
    {
        std::string amino = JAV_AMINO[i].szSymbol;
        if(caseInsCompare(amino.substr(1,3).c_str(), resname))
           return (i+1);
    }
    return 0;
}

const char *get_nth_element(int num)
{
    if((num >=0) && (num < N_ELEMENTS))
       return JAV_ELEMENTY[num].szSymbol;
    return "Q";
}

const char *get_nth_element_name(int num)
{
    if((num >=0) && (num < N_ELEMENTS))
       return JAV_ELEMENTY[num].szString;
    return "Q";
}

int IsDNA(const std::string &resname, const std::string &elem_name)
{
    static const char *elem_DNA[] = {"h", "c", "n", "o", "p"};
    int N_elem_DNA = (int)(sizeof(elem_DNA)/sizeof(elem_DNA[0]));
    if(resname.empty())
       return 0;
    for(int i=0; i<N_DNA_NUCLEOTIDES; i++)
    {
        std::string amino = JAV_DNA_NUCLEOTIDES[i].szSymbol;
        if(caseInsCompare(amino.substr(2,2), resname))
           return (i+1+N_AMINO);
    }
    for(int i=0; i<N_DNA_NUCLEOTIDES; i++)
    {
        std::string amino = JAV_DNA_NUCLEOTIDES[i].szCode;
        if(caseInsCompare(amino, resname))
           return (i+1+N_AMINO);
    }
    if(resname.length() <= 1)
       return 0;
    if(!elem_name.empty())
    {
       bool found = false;
       for(int i=0; i<N_elem_DNA; i++)
            if(strcmp(elem_name.c_str(), elem_DNA[i]) == 0)
              found = true;
       if(!found)
          return 0;
    }
    char c = toupper(resname[1]);
    for(int i=0; i<N_DNA_NUCLEOTIDES; i++)
    {
        char c1 = JAV_DNA_NUCLEOTIDES[i].szCode[0];
        if(c == c1)
           return (i+1+N_AMINO);
    }
    return 0;
}

bool IsWater(const std::string &resname)
{
    if(resname.empty())
       return false;
    if(caseInsCompare("HOH", resname))
       return true;
    if(caseInsCompare("WAT", resname))
       return true;
    return false;
}

int IsAminoNucle(const std::string &resname, const std::string &elem_name)
{
    int n = IsAmino(resname);
    if(n)
       return(n);
    n = IsDNA(resname, elem_name);
    if(n)
       return(n);
    if(IsWater(resname))
       return(N_AMINO+N_DNA_NUCLEOTIDES+1);
    return (N_AMINO+N_DNA_NUCLEOTIDES+1);
}

const char *get_AminoCode(int num)
{
    if((num >0) && (num <= N_AMINO))
       return (JAV_AMINO[num-1].szCode);
    else if((num >N_AMINO) && (num <= (N_AMINO+N_DNA_NUCLEOTIDES)))
       return (JAV_DNA_NUCLEOTIDES[num-N_AMINO-1].szCode);
    return "X";
}

bool CheckAtom(const std::string &str)
{
  static const char *Atom[] = {
    "AD1", "AD2", "AE1", "AE2", "C", "CA", "CB", "CD", "CD1", "CD2", "CE", "CE1", "CE2",
    "CE3", "CG", "CG1", "CG2", "CH2", "CH3", "CZ", "CZ2", "CZ3", "HG", "HG1", "HH", "HH2",
    "HZ", "HZ2", "HZ3", "N", "ND1", "ND2", "NE", "NE1", "NE2", "NH1", "NH2", "NZ", "O",
    "OD1", "OD2", "OE", "OE1", "OE2", "OG", "OG1", "OH", "OXT", "SD", "SG", "H", "HA", "HB",
    "HD1", "HD2", "HE", "HE1", "HE2", "HE3", "1H", "1HA", "1HB", "1HD", "1HD1", "1HD2",
    "1HE", "1HE2", "1HG", "1HG1", "1HG2", "1HH1", "1HH2", "1HZ", "2H", "2HA", "2HB", "2HD",
    "2HD1", "2HD2", "2HE", "2HE2", "2HG", "2HG1", "2HG2", "2HH1", "2HH2", "2HZ", "3H", "3HB",
    "3HD1", "3HD2", "3HE", "3HG1", "3HG2", "3HZ",
     "O3'", "O4'", "O5'",
     "C1'", "C2'", "C3'", "C4'", "C5'", "OP1", "OP2",
     "N1", "N2", "N3", "N4", "N6", "O2", "O4", "O6"//,
//     "O3*", "O4*", "O5*","C1*", "C2*", "C3*", "C4*", "C5*"
    };

  if(str.empty())
     return false;
  for( int i=0; i<(int)(sizeof(Atom)/sizeof(Atom[0])); i++ )
    if( caseInsCompare(str,Atom[i]) )
        return true;
  return (is_valid_symbol(str) != 0);
}

bool IsProline(std::string &resname)
{
    if(resname.empty())
       return false;
    if(resname.compare("PRO") == 0)
       return true;
    return false;
}

float Minimo (float vett[], int n)
{
   return *std::min_element(vett, vett+n);
}

double Minimo (const QVector<double> &vett)
{
    return *std::min_element(vett.constBegin(), vett.constEnd());
}

float Massimo (float vett[], int n)
{
   return *std::max_element(vett, vett+n);
}

double Massimo (const QVector<double> &vett)
{
    return *std::max_element(vett.constBegin(), vett.constEnd());
}

QDataStream& operator<<(QDataStream& out, const _LightInfo& v) {
    out << v.enabled << v.diffuse << v.specular << v.position[0]
	    << v.position[1] << v.position[2] << v.position[3];
    return out;
}

QDataStream& operator>>(QDataStream& in, _LightInfo& v) {
    in >> v.enabled >> v.diffuse >> v.specular >> v.position[0]
	    >> v.position[1] >> v.position[2] >> v.position[3];
    return in;
}

