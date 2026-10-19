// Geometry details share the collectible's mesh, silhouette, catalogue and collision.
void BevelBox(FVector P,FVector H,float Bevel,int Mat)
{
    const FVector2D Outline[]={{-H.Y+Bevel,-H.Z},{H.Y-Bevel,-H.Z},{H.Y,-H.Z+Bevel},{H.Y,H.Z-Bevel},{H.Y-Bevel,H.Z},{-H.Y+Bevel,H.Z},{-H.Y,H.Z-Bevel},{-H.Y,-H.Z+Bevel}};
    int V[4][8];
    for(int R=0;R<4;++R)for(int J=0;J<8;++J)
    {
        const float X=R==0?-H.X:R==1?-H.X+Bevel:R==2?H.X-Bevel:H.X;
        const float Inset=(R==0||R==3)?Bevel*.55f:0.f;
        const auto O=Outline[J];
        V[R][J]=Mesh.AppendVertex(P+FVector(X,O.X-FMath::Sign(O.X)*Inset,O.Y-FMath::Sign(O.Y)*Inset));
    }
    for(int R=0;R<3;++R)for(int J=0;J<8;++J){int K=(J+1)%8;Tri(V[R][J],V[R+1][K],V[R+1][J],Mat);Tri(V[R][J],V[R][K],V[R+1][K],Mat);}
    for(int J=1;J<7;++J){Tri(V[0][0],V[0][J+1],V[0][J],Mat);Tri(V[3][0],V[3][J],V[3][J+1],Mat);}
}
void Lettering(const FString& Text,FVector P,float Pixel,int Mat,bool OnTop=false,float WrapRadius=0)
{
    static const TMap<TCHAR,FString> Font={
        {'0',TEXT("01110/10001/10011/10101/11001/10001/01110")},{'1',TEXT("00100/01100/00100/00100/00100/00100/01110")},
        {'2',TEXT("01110/10001/00001/00010/00100/01000/11111")},{'3',TEXT("11110/00001/00001/01110/00001/00001/11110")},
        {'4',TEXT("00010/00110/01010/10010/11111/00010/00010")},{'5',TEXT("11111/10000/10000/11110/00001/00001/11110")},
        {'6',TEXT("01110/10000/10000/11110/10001/10001/01110")},{'7',TEXT("11111/00001/00010/00100/01000/01000/01000")},
        {'8',TEXT("01110/10001/10001/01110/10001/10001/01110")},{'9',TEXT("01110/10001/10001/01111/00001/00001/01110")},
        {'A',TEXT("01110/10001/10001/11111/10001/10001/10001")},{'B',TEXT("11110/10001/10001/11110/10001/10001/11110")},
        {'C',TEXT("01111/10000/10000/10000/10000/10000/01111")},{'D',TEXT("11110/10001/10001/10001/10001/10001/11110")},
        {'E',TEXT("11111/10000/10000/11110/10000/10000/11111")},{'F',TEXT("11111/10000/10000/11110/10000/10000/10000")},
        {'G',TEXT("01111/10000/10000/10111/10001/10001/01111")},{'H',TEXT("10001/10001/10001/11111/10001/10001/10001")},
        {'I',TEXT("11111/00100/00100/00100/00100/00100/11111")},{'K',TEXT("10001/10010/10100/11000/10100/10010/10001")},
        {'L',TEXT("10000/10000/10000/10000/10000/10000/11111")},{'M',TEXT("10001/11011/10101/10101/10001/10001/10001")},
        {'N',TEXT("10001/11001/10101/10011/10001/10001/10001")},{'O',TEXT("01110/10001/10001/10001/10001/10001/01110")},
        {'P',TEXT("11110/10001/10001/11110/10000/10000/10000")},{'R',TEXT("11110/10001/10001/11110/10100/10010/10001")},
        {'S',TEXT("01111/10000/10000/01110/00001/00001/11110")},{'T',TEXT("11111/00100/00100/00100/00100/00100/00100")},
        {'U',TEXT("10001/10001/10001/10001/10001/10001/01110")},{'V',TEXT("10001/10001/10001/10001/10001/01010/00100")},
        {'X',TEXT("10001/10001/01010/00100/01010/10001/10001")},{'Y',TEXT("10001/10001/01010/00100/00100/00100/00100")},
        {'+',TEXT("00000/00100/00100/11111/00100/00100/00000")},{'-',TEXT("00000/00000/00000/11111/00000/00000/00000")},
        {'=',TEXT("00000/11111/00000/11111/00000/00000/00000")},{'>',TEXT("10000/01000/00100/00010/00100/01000/10000")},
        {'.',TEXT("00000/00000/00000/00000/00000/00110/00110")},{'/',TEXT("00001/00001/00010/00100/01000/10000/10000")}
    };
    for(int I=0;I<Text.Len();++I)if(const auto* Pattern=Font.Find(Text[I]))
        for(int R=0;R<7;++R)for(int C=0;C<5;++C)if((*Pattern)[R*6+C]=='1')
        {
            const float Y=(I*6+C)*Pixel,Z=-R*Pixel;
            FVector Center=P+(OnTop?FVector(Z,Y,0):FVector(0,Y,Z));
            if(WrapRadius>0)Center.X=-FMath::Sqrt(FMath::Max(1.f,WrapRadius*WrapRadius-float(Center.Y*Center.Y)))-.18f;
            Box(Center,OnTop?FVector(Pixel*.49f,Pixel*.49f,.10f):FVector(.10f,Pixel*.49f,Pixel*.49f),Mat);
        }
}
void DetailedComputer()
{
    BevelBox({9,0,14},{26,40,32},3,0); // Deep CRT housing and separate front bezel.
    BevelBox({-18,0,15},{3.5,37,28},1.8,0);
    BevelBox({-22,0,17},{1.4,31.5,22},1.2,1);
    BevelBox({-23.5,0,17},{.8,29,20},.6,2);
    for(int S:{-1,1})for(int J=0;J<9;++J)Box({10+J*2.1,S*40.05,17},{.55,.15,13},1);
    for(int J=0;J<8;++J)Box({35.05,-24+J*6.,18},{.15,1.1,16},1);
    BevelBox({7,0,-23},{9,11,7},2,6);BevelBox({4,0,-32},{22,29,3.5},2,0);
    Lettering(TEXT("ANT OS"),{-22.0,-31,-8},.5,1);
    Ellipsoid({-22.2,29,-7},{.5,1.2,1.2},2,12,6);
    for(int I=0;I<3;++I)BevelBox({-22.1,17+I*3.2,-7},{.4,1,1},.3,6);
    Lettering(TEXT("COLONY 01"),{-24.45,-25,32},.50,0);
    const FVector2D Chambers[]={{0,23},{-17,15},{13,11},{-7,0},{19,-1}};
    for(const auto P:Chambers)Ellipsoid({-24.5,P.X,P.Y},{.35,4.8,2.6},5,20,8);
    for(int I=1;I<5;++I)Rod({-24.5,Chambers[(I-1)/2].X,Chambers[(I-1)/2].Y},{-24.5,Chambers[I].X,Chambers[I].Y},.8,5);
    for(int I=0;I<7;++I)
    {
        const float Y=-18+I*5,Z=17-I*2;
        for(int J=0;J<3;++J)Ellipsoid({-25,Y+J*.8,Z},{.3,.55,.4},1,8,5);
        for(int S:{-1,1})for(int J=0;J<3;++J)Rod({-25,Y+J*.65,Z},{-25.1,Y+J*.65-.4,Z+S*1.1},.12,1);
    }
    // Keyboard has sculpted keys, a real space bar and top-facing legends.
    BevelBox({-28,0,-46},{18,41,4},2,0);
    const TCHAR* Rows[]={TEXT("1234567890"),TEXT("ABCDEFGHIJ"),TEXT("KLMNOPRSTU")};
    for(int R=0;R<3;++R)for(int C=0;C<10;++C)
    {
        const FVector P(-19-R*7.,-34+C*7.4,-40.5);
        BevelBox(P,{2.8,3,1.3},.65,6);
        Lettering(FString::Chr(Rows[R][C]),P+FVector(.8,-.8,1.45),.32,1,true);
    }
    BevelBox({-41,0,-40.5},{2.3,15,1.3},.6,6);
    for(int S:{-1,1})BevelBox({-41,S*29.,-40.5},{2.3,8,1.3},.6,6);
    Curve({-12,33,-46},{10,58,-46},{26,50,-38},{22,36,-25},.6,1,24);
    for(int S:{-1,1})for(int Z:{-4,32})Ellipsoid({35.1,S*32.,double(Z)},{.3,1,1},6,12,6);
}
void DetailedCalculator()
{
    BevelBox({0,0,0},{10,36,56},3,1);BevelBox({-10,0,0},{1.6,34.7,54.5},1,6);
    BevelBox({-12.2,0,29},{1,31,15},1.5,1);BevelBox({-13.3,0,29},{.3,28.5,12.5},.3,2);
    Lettering(TEXT("1+1>2"),{-13.8,-24,34},1.65,1);
    Lettering(TEXT("MIND  12 DIGIT"),{-12,-29,49},.48,0);
    for(int I=0;I<4;++I){BevelBox({-12.3,-23+I*7.8,9},{.3,3.4,2.4},.2,5);Box({-12.7,-23+I*7.8,9},{.1,.18,2.1},4);}
    Lettering(TEXT("SOLAR"),{-12.4,12,10},.42,1);
    const TCHAR* Keys[]={TEXT("7"),TEXT("8"),TEXT("9"),TEXT("/"),TEXT("4"),TEXT("5"),TEXT("6"),TEXT("X"),TEXT("1"),TEXT("2"),TEXT("3"),TEXT("-"),TEXT("0"),TEXT("."),TEXT("="),TEXT("+")};
    for(int R=0;R<4;++R)for(int C=0;C<4;++C)
    {
        const float Y=-25.5+C*17,Z=-3-R*13.5;
        BevelBox({-13.4,Y,Z},{2.2,6.7,4.8},1, C==3?7:0);
        Lettering(Keys[R*4+C],{-15.8,Y-1.5f,Z+2.4f},.68,C==3?0:1);
    }
    for(int S:{-1,1})for(int Z:{-46,46})
    {Ellipsoid({10.05,S*27.,double(Z)},{.3,1.8,1.8},0,12,6);Box({10.4,S*27.,double(Z)},{.1,1.1,.22},1);}
    BevelBox({10.3,0,-19},{.5,24,20},1,6);
    for(int S:{-1,1})for(int Z:{-48,48})BevelBox({11,S*27.,double(Z)},{1,3,2},.5,1);
}
void SmoothRing(FVector P,FVector Radius,float Tube,int Mat)
{
    int V[64][8];
    for(int I=0;I<64;++I)for(int J=0;J<8;++J)
    {
        const double A=2*PI*I/64,B=2*PI*J/8;
        V[I][J]=Mesh.AppendVertex(P+FVector((Radius.X+Tube*FMath::Cos(B))*FMath::Cos(A),(Radius.Y+Tube*FMath::Cos(B))*FMath::Sin(A),Tube*FMath::Sin(B)));
    }
    for(int I=0;I<64;++I)for(int J=0;J<8;++J){int N=(I+1)%64,K=(J+1)%8;Tri(V[I][J],V[N][J],V[N][K],Mat);Tri(V[I][J],V[N][K],V[I][K],Mat);}
}
void DetailedCola()
{
    const FVector2D Profile[]={{17.5,-44},{19.5,-43},{20.5,-40},{22.5,-35},{23,-30},{23,25},{22.4,29},{20,33},{19.5,35},{20,36}};
    constexpr int Sides=64;int V[10][Sides];
    for(int R=0;R<10;++R)for(int I=0;I<Sides;++I)
    {float A=I*2*PI/Sides;V[R][I]=Mesh.AppendVertex({Profile[R].X*FMath::Cos(A),Profile[R].X*FMath::Sin(A),Profile[R].Y});}
    for(int R=0;R<9;++R)for(int I=0;I<Sides;++I)
    {int J=(I+1)%Sides;int M=(R<2||R>6)?6:7;Tri(V[R][I],V[R][J],V[R+1][J],M);Tri(V[R][I],V[R+1][J],V[R+1][I],M);}
    Cylinder({0,0,35.1},19.5,1,6,64);Cylinder({0,0,-43},17.5,1,6,64);
    for(int Z:{-42,35,37})SmoothRing({0,0,double(Z)},{19.8,19.8,0},.65,6);
    Ellipsoid({-6,0,36.1},{6.5,4.3,.25},1,32,8);
    SmoothRing({4,0,37},{5.6,3.5,0},1.2,6);Cylinder({1,0,37},1.1,1.5,6,16);
    // Label follows the curved can; no flat plaque floating ahead of the cylinder.
    Lettering(TEXT("MIND"),{-23,-13,19},1.15,0,false,23);
    Lettering(TEXT("COLA"),{-23,-17,7},1.48,0,false,23);
    Lettering(TEXT("330 ML"),{-23,-10,-24},.55,0,false,23);
    for(int I=0;I<40;++I)
    {
        float A=(I/39.f-.5f)*1.65f,Z=-11+3*FMath::Sin(A*3);
        Ellipsoid({-23.05*FMath::Cos(A),23.05*FMath::Sin(A),Z},{.45,.6,1.0},0,8,5);
    }
    for(int I=0;I<22;++I){float A=I*2.39996f;float Z=-28+(I*17%51);Ellipsoid({23.3*FMath::Cos(A),23.3*FMath::Sin(A),Z},{.38,.38,.65},6,8,5);}
    for(int I=0;I<14;++I)Box({23.1,-7.+I, -17},{.12,.22,5.+(I%3)},0);
    Curve({-6,0,38},{-5,0,65},{-18,-3,70},{-23,-3,54},2.1f,5,24);
    for(int J=0;J<3;++J)Ellipsoid({-24.-J*2,-3.,50.-J*6},{.8,.8,1.6},5,16,8);
}
