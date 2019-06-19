x=100;
y=10;
if(...)
{
    x=200;
    //illegal.memory_op
}
z = x*10;

x -> {D1,D3}
y -> {D2}
z -> {D4}





