// Native low-poly miniatures, sharing the existing inspect/pickup renderer.
void BranchCollectible(int Id)
{
    if(Id==16)
    {
        Ellipsoid({0,0,-43},{31,43,4},0,32,8);
        for(int S=-1;S<=1;++S)
        {
            Rod({-18,float(S*20),-42},{18,float(S*20),49},1.4f,5);
            for(int J=0;J<4;++J)
            {
                FVector P(-10+J*7,S*20,-23+J*18);
                // Irregular flattened cuts of beef, with pale fat seams and char.
                const int First=Mesh.MaxVertexID();
                Box(P,{5.5f,float(8+(J%2)*2),float(6+(J%3))},J%2?5:7);
                const FQuat Twist(FVector::ForwardVector,(S*11+J*17)*PI/180.f);
                for(int V=First;V<Mesh.MaxVertexID();++V)
                {
                    FVector Q=FVector(Mesh.GetVertex(V))-P;
                    Q.Y+=(V%3-1)*1.3f;Q.Z+=(V%2?1:-1)*1.1f;
                    Mesh.SetVertex(V,P+Twist.RotateVector(Q));
                }
                Rod(P+FVector(-6,-7,-3),P+FVector(-6,5,-1),.65f,0);
                for(int K=-1;K<=1;++K)Rod(P+FVector(-6,K*3,-4),P+FVector(-6,K*3,4),.8f,1);
            }
        }
    }
    else if(Id==17)
    {
        // Open spherical glass bowl; a curved silhouette instead of rectangular panes.
        constexpr int Sides=48,Rings=24;
        int V[Rings+1][Sides];
        for(int R=0;R<=Rings;++R)for(int S=0;S<Sides;++S)
        {
            const float T=FMath::Lerp(.80f,2.78f,float(R)/Rings),A=S*2*PI/Sides;
            V[R][S]=Mesh.AppendVertex(FVector(46*FMath::Sin(T)*FMath::Cos(A),46*FMath::Sin(T)*FMath::Sin(A),46*FMath::Cos(T)));
        }
        for(int R=0;R<Rings;++R)for(int S=0;S<Sides;++S)
        {int N=(S+1)%Sides;Tri(V[R][S],V[R+1][S],V[R+1][N],8);Tri(V[R][S],V[R+1][N],V[R][N],8);}
        Ring({0,0,32},{33,33,0},1.5f,8);Cylinder({0,0,-44},17,3,8,32);
        Cylinder({0,0,-32},30,3,4,32);
        Cylinder({0,0,24},38,0.6f,8,48);
        for(int I=0;I<7;++I)
        {
            const float Angle=I*2.39996f, Height=20+(I*17)%26, Bend=FMath::Sin(I*1.7f)*8;
            FVector P(11+FMath::Cos(Angle)*9,FMath::Sin(Angle)*24,-30);
            Curve(P,P+FVector(-3,Bend,Height*.28f),P+FVector(5,-Bend,Height*.65f),P+FVector(-4,Bend*.5f,Height),.65f+(I%3)*.26f,2);
            for(int J=0;J<2+I%3;++J)
                Ellipsoid(P+FVector(-2,(J%2?1:-1)*(3+I%4),Height*(.25f+J*.15f)),{1.f,3.f+I%4,1.5f},2,8,4);
        }
        for(int I=0;I<15;++I)Ellipsoid({float((I%3)*14-14),float((I*13)%42-21),-29},{4,5,2},I%2?6:0,8,4);
    }
    else if(Id==18)
    {
        Ellipsoid({0,0,-6},{33,39,43},5,16,10);
        Ellipsoid({0,0,28},{23,28,12},5,16,8);
        Ring({0,0,31},{24,29,0},2,1);Ring({0,0,34},{23,28,0},2,4);
        // Open mouth filled with stacked coins and coins spilled on the plinth.
        Cylinder({0,0,36},22,3,1,24);
        for(int I=0;I<22;++I)
        {
            const float A=I*2.39996f,R=4+(I%4)*4;
            Cylinder({R*FMath::Cos(A),R*FMath::Sin(A),38.f+(I%3)*2},5,1.8f,4,16);
        }
        for(int I=0;I<7;++I)Cylinder({-30.f+I*9,-33.f+(I%3)*8,-47},6,2,4,16);
        // A bold coin emblem on the front of the bag.
        Ellipsoid({-32,0,-2},{1.2,13,13},4,24,12);
        Rod({-34,0,-12},{-34,0,8},1,5);Rod({-34,-6,4},{-34,6,4},1,5);
    }
    else if(Id==19)
    {
        Cylinder({0,0,-46},22,3,8,32);Rod({0,0,-44},{0,0,-14},2,8);
        // Layered bowl taper, coloured drink and a citrus wheel.
        for(int I=0;I<40;++I)
        {
            const float A=I*2*PI/40,B=(I+1)*2*PI/40;
            int V[4];V[0]=Mesh.AppendVertex(FVector(7*FMath::Cos(A),7*FMath::Sin(A),-13));V[1]=Mesh.AppendVertex(FVector(33*FMath::Cos(A),33*FMath::Sin(A),28));
            V[2]=Mesh.AppendVertex(FVector(33*FMath::Cos(B),33*FMath::Sin(B),28));V[3]=Mesh.AppendVertex(FVector(7*FMath::Cos(B),7*FMath::Sin(B),-13));
            Tri(V[0],V[1],V[2],8);Tri(V[0],V[2],V[3],8);
        }
        for(int I=0;I<40;++I)
        {
            const float A=I*2*PI/40,B=(I+1)*2*PI/40;int V[4];
            V[0]=Mesh.AppendVertex(FVector(10*FMath::Cos(A),10*FMath::Sin(A),-8));V[1]=Mesh.AppendVertex(FVector(28*FMath::Cos(A),28*FMath::Sin(A),22));
            V[2]=Mesh.AppendVertex(FVector(28*FMath::Cos(B),28*FMath::Sin(B),22));V[3]=Mesh.AppendVertex(FVector(10*FMath::Cos(B),10*FMath::Sin(B),-8));
            Tri(V[0],V[1],V[2],10);Tri(V[0],V[2],V[3],10);
        }
        Cylinder({0,0,22},28,1,10,40);Ring({0,0,28},{33,33,0},1.2f,8);
        Ellipsoid({0,29,25},{2,11,11},4,24,12);Ellipsoid({-2.3,29,25},{.4,8,8},0,20,10);
        for(int I=0;I<8;++I){float A=I*PI/4;Rod({-3,29,25},{-3,29+8*FMath::Cos(A),25+8*FMath::Sin(A)},.35f,4);}
        Rod({7,-16,4},{7,-28,53},1.2f,0);
    }
    else if(Id==20)
    {
        Box({7,0,-50},{18,33,4},5);Rod({7,0,-46},{7,0,-26},3,4);
        Ellipsoid({3,0,0},{5,45,45},4,48,24);
        constexpr int Rings=22,Sides=96;
        for(int R=0;R<Rings;++R)for(int S=0;S<Sides;++S)
        {
            FVector P[4];const float A=S*2*PI/Sides,C=(S+1)*2*PI/Sides;
            P[0]={-3,R*2.f*FMath::Cos(A),R*2.f*FMath::Sin(A)};P[1]={-3,(R+1)*2.f*FMath::Cos(A),(R+1)*2.f*FMath::Sin(A)};
            P[2]={-3,(R+1)*2.f*FMath::Cos(C),(R+1)*2.f*FMath::Sin(C)};P[3]={-3,R*2.f*FMath::Cos(C),R*2.f*FMath::Sin(C)};
            const FVector Mid=(P[0]+P[1]+P[2]+P[3])*.25f;
            bool Dark=Mid.Y>0;
            if(FVector2D(Mid.Y,Mid.Z-22).Size()<22)Dark=false;
            if(FVector2D(Mid.Y,Mid.Z+22).Size()<22)Dark=true;
            int V[4];for(int I=0;I<4;++I)V[I]=Mesh.AppendVertex(P[I]);
            if(R)Tri(V[0],V[3],V[1],Dark?1:0);Tri(V[1],V[3],V[2],Dark?1:0);
        }
        Ellipsoid({-4,0,22},{.7,5,5},1,24,12);Ellipsoid({-4,0,-22},{.7,5,5},0,24,12);
    }
    else if(Id==21)
    {
        Box({0,0,-34},{28,86,5},1);Box({0,0,35},{31,88,4},0);
        Box({27,0,0},{2,85,32},6);for(int Y:{-1,1})Box({0,float(Y*86),0},{29,2,32},0);
        // Front side stays open at the middle doors so the crowd can pour out.
        for(int Y:{-1,1})
        {
            Box({-28,float(Y*55),-20},{2,30,14},0);Box({-28,float(Y*55),29},{2,30,5},0);
            for(int K:{-1,1})Box({-28,float(Y*55+K*28),8},{2,2,18},0);
            Box({-29,float(Y*55),8},{.5,24,15},3);Box({-30,float(Y*55),-11},{.7,30,2},2);
            Box({15,float(Y*54),-12},{8,27,4},2);
            for(int K:{-1,1})Ellipsoid({float(K*21),float(Y*56),-41},{6,12,10},1,16,8);
        }
        Box({-29,0,28},{2,23,5},2);Box({-31,0,28},{.5,12,1},0);
        for(int I=-3;I<=3;++I){Rod({8,float(I*21),29},{8,float(I*21),17},.8f,4);Ring({8,float(I*21),15},{3,3,0},.7f,4);}
    }
    else if(Id==22)
    {
        Box({0,0,0},{6,33,58},1);Box({-6.5,0,0},{.8,30,53},6);
        Box({-8,0,49},{.5,11,2},1);Ellipsoid({-8,-16,49},{.5,1.5,1.5},1,12,8);
        Box({-8,0,-49},{.5,9,1},0);
        for(int I=0;I<4;++I)Ellipsoid({-8,float(-21+I*14),-39},{.7,3,3},I%2?2:4,12,6);
        Box({0,34,20},{3,1,9},6);
    }
}
