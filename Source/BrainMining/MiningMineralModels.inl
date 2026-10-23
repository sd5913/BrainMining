// All four rewards share the game's faceted, hand-built collectible language.
// Front faces look along -X, matching the catalogue inspection camera.
void FrontRing(FVector P,float R,float Tube,int Mat,int Steps=48)
{
    for(int I=0;I<Steps;++I)
    {const float A=I*2*PI/Steps,C=(I+1)*2*PI/Steps;Rod(P+FVector(0,R*FMath::Cos(A),R*FMath::Sin(A)),P+FVector(0,R*FMath::Cos(C),R*FMath::Sin(C)),Tube,Mat);}
}
void MineralReward(int32 Id)
{
    if(Id==12)
    {
        Cylinder({0,0,-51},32,8,1,32);Cylinder({0,0,-46},26,4,5,32);
        Cylinder({0,0,-33},13,24,5,24);
        for(int Z=-43;Z<-20;Z+=4)Ring({0,0,float(Z)},{13,13,0},1.2f,5);
        Ellipsoid({0,0,15},{29,29,35},8,32,18);
        Cylinder({0,0,-17},15,8,5,24);
        Rod({0,-7,-18},{0,-7,9},.9f,4);Rod({0,7,-18},{0,7,9},.9f,4);
        // A small brain-shaped filament inside the bulb.
        for(int Side:{-1,1})
        {
            Curve({-6,float(Side)*3,0},{-6,float(Side)*27,5},{-6,float(Side)*24,30},{-6,float(Side)*6,32},1.2f,4,20);
            Curve({-6,float(Side)*6,32},{-6,float(Side)*-4,27},{-6,float(Side)*7,10},{-6,float(Side)*3,0},1.2f,4,20);
            Curve({-7,float(Side)*5,10},{-7,float(Side)*19,7},{-7,float(Side)*9,25},{-7,float(Side)*18,24},.8f,4,16);
        }
        // Tangled wire underneath eventually closes a visible outer circuit.
        for(int I=0;I<100;++I)
        {
            auto Wire=[](float T){return FVector(FMath::Cos(T)*24,FMath::Sin(T)*37,-43+FMath::Sin(T*3)*5);};
            const float T=I*4*PI/100;Rod(Wire(T),Wire(T+4*PI/100),.85f,5);
        }
        Curve({-15,-20,-44},{-30,-39,-27},{-29,-34,8},{-9,-7,9},1.1f,5);
        Curve({-9,7,9},{-29,34,8},{-30,39,-27},{-15,20,-44},1.1f,5);
    }
    else if(Id==13)
    {
        Box({8,0,-52},{18,33,5},1);Rod({8,-24,-48},{8,-24,-25},2,6);Rod({8,24,-48},{8,24,-25},2,6);
        Ellipsoid({6,0,0},{7,46,46},1,32,20);
        FrontRing({-2,0,0},45,2.5f,5);FrontRing({-3,0,0},40,1,6);
        for(int I=0;I<32;++I)
        {const float A=I*PI/16;Rod({-5,37*FMath::Cos(A),37*FMath::Sin(A)},{-5,(I%4==0?31.f:34.f)*FMath::Cos(A),(I%4==0?31.f:34.f)*FMath::Sin(A)},I%4==0?.9f:.45f,0);}
        // N at twelve o'clock; no authored text texture needed.
        Rod({-6,-3,31},{-6,-3,38},.6f,7);Rod({-6,-3,38},{-6,3,31},.6f,7);Rod({-6,3,31},{-6,3,38},.6f,7);
        Ellipsoid({-10,0,0},{2.5,3,3},4,12,8);
        FrontRing({6,0,52},7,1.8f,5,24);
    }
    else if(Id==14)
    {
        Box({6,0,-52},{20,42,5},1);Box({6,0,-46},{14,35,2},5);
        Ellipsoid({7,0,0},{10,48,47},2,24,16);
        // Real layered geometry: parallel mineral bands become distant ridges
        // when viewed head-on and separate into strata when rotated.
        for(int Band=0;Band<9;++Band)
        {
            const int Mat=Band%3==0?0:Band%3==1?2:6;
            const float Base=-36+Band*8,Width=FMath::Sqrt(FMath::Max(0.f,1-FMath::Square(Base/50)))*43;
            for(int I=0;I<32;++I)
            {
                auto P=[&](int J,float Offset)
                {
                    float Y=FMath::Lerp(-Width,Width,J/32.f),Z=Base+Offset+5*FMath::Sin(Y*.085f+Band*.65f)+2*FMath::Sin(Y*.23f);
                    const float Edge=FMath::Sqrt(FMath::Square(Y/48)+FMath::Square(Z/47));
                    if(Edge>.94f){Y*=.94f/Edge;Z*=.94f/Edge;}
                    return FVector(-4.5f-Band*.15f,Y,Z);
                };
                const int A=Mesh.AppendVertex(P(I,0)),C=Mesh.AppendVertex(P(I+1,0)),D=Mesh.AppendVertex(P(I+1,7)),E=Mesh.AppendVertex(P(I,7));
                Tri(A,D,C,Mat);Tri(A,E,D,Mat);
            }
        }
        Curve({-8,-26,-34},{-8,37,-18},{-8,-27,0},{-8,9,17},1.2f,0,42);
        // Tiny mountain trail and a solitary tree give the bands their scale.
        Curve({-8,32,-32},{-8,10,-18},{-8,30,-8},{-8,16,1},.6f,4,24);
        Rod({-8,-25,-5},{-8,-25,8},.6f,1);
        for(int I=0;I<3;++I){Rod({-8,-25,float(I*4)},{-8,float(-30+I),float(I*4+4)},.5f,1);Rod({-8,-25,float(I*4)},{-8,float(-20-I),float(I*4+4)},.5f,1);}
    }
    else if(Id==15)
    {
        Box({12,0,-49},{40,27,5},1);Rod({18,0,-44},{18,0,-12},3,5);
        // Faceted telescope barrel. The diamond lens and its light pattern
        // are visible together, rather than looking down an opaque tube.
        for(int I=0;I<8;++I)
        {
            const float A=I*PI/4,C=(I+1)*PI/4;
            const FVector V0(-12,17*FMath::Cos(A),17*FMath::Sin(A)),V1(53,17*FMath::Cos(A),17*FMath::Sin(A));
            const FVector V2(53,17*FMath::Cos(C),17*FMath::Sin(C)),V3(-12,17*FMath::Cos(C),17*FMath::Sin(C));
            const int X=Mesh.AppendVertex(V0),Y=Mesh.AppendVertex(V1),Z=Mesh.AppendVertex(V2),W=Mesh.AppendVertex(V3);
            Tri(X,Z,Y,I%2?3:1);Tri(X,W,Z,I%2?3:1);
            Rod(V0,V1,.65f,5);
        }
        FrontRing({-13,0,0},18,2,5);FrontRing({48,0,0},18,2,5);FrontRing({54,0,0},10,1.5f,0);
        Ellipsoid({-18,0,0},{10,13,13},0,8,4);
        for(int I=0;I<6;++I)
        {const float A=I*PI/3;Rod({-20,10*FMath::Cos(A),10*FMath::Sin(A)},{-42,34*FMath::Cos(A),34*FMath::Sin(A)},.35f,6);}
        FrontRing({-44,0,0},42,.45f,6);
    }
}
