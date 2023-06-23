#define CUBE_DIMENSION 5 //1 to 8
#define DATA_PIN 8 //DS
#define LATCH_PIN 9 //STCP
#define CLOCK_PIN 10 //SHCP
#define SCHEMA_BUG << 1 //a bug in my scheme, remove it if the scheme is correct

#define CUBE_RENDER_LAYER_DELAY 500 //micros
#define CUBE_CONTROLLER_DELAY 10 //millis

#define RAIN_EFFECT_DELAY 70 //millis

#define PONG_EFFECT_DELAY 40 //millis
#define PONG_ELEMENT_COUNT 1
#define PONG_LAIL_LENGHT 5
#define PONG_THRESHOLD 20 //%

#define BREATH_DELAY 70 //millis
#define BREATH_END_DELAY 1400 //millis

enum AppState : int
{
  FullMatrixOff = 0,
  FullMatrixOn = 1,
  RainEffect = 2,
  PongEffect = 3,
  BreathEffect = 4,
  TestEffect = 5
};

struct Point
{
  int8_t X;
  int8_t Y;
  int8_t Z;
};


const int MaxCubeLenght2 = CUBE_DIMENSION * CUBE_DIMENSION;
const int MaxCubeLenght = CUBE_DIMENSION * CUBE_DIMENSION * CUBE_DIMENSION;
uint8_t CubeBuffer[CUBE_DIMENSION][CUBE_DIMENSION];
AppState CurrentAppState = FullMatrixOff;


//-------------------- common
void GetCoordinateFromIndex(int index, int8_t *x, int8_t *y, int8_t *z)
{
  int tmp = index % MaxCubeLenght2;

  *x = index / MaxCubeLenght2;
  *y = tmp / CUBE_DIMENSION;
  *z = tmp % CUBE_DIMENSION;
}

void FastSetPin(uint8_t pin, uint8_t val) 
{ 	
	if (pin < 8) 
    bitWrite(PORTD, pin, val);
	else if (pin < 14) 
    bitWrite(PORTB, (pin - 8), val); 
	else if (pin < 20) 
    bitWrite(PORTC, (pin - 14), val);
}

void FastShiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
  for (uint8_t i = 0; i < 8; ++i)  
  {
    if (bitOrder == LSBFIRST)
      FastSetPin(dataPin, (val & (1 << i)));
    else    
      FastSetPin(dataPin, (val & (1 << (7 - i))));

    FastSetPin(clockPin, HIGH);
    FastSetPin(clockPin, LOW);        
  }
}

template<typename T>
void SetArrRank1(T *arr, int rank, T value)
{
  for (int i = 0; i < rank; ++i)
    arr[i] = value;
}

template<typename T>
void SetArrRank2(T *arr, int rank1, int rank2, T value)
{
  for (int i = 0; i < rank1; ++i)
    SetArrRank1<T>(&arr[i * rank1], rank2, value);
}
//-------------------- end common


//-------------------- time worker
class TimeWorker
{
  private:
    unsigned short _delay;
    void (*_clbk)(bool);
    unsigned long _lastExecTime;
    bool *_eventInvokeFlag;
    bool _onlyEventInvoked;
    bool _useMicros;

  public:
    //eventInvokeFlag - clbk вызывается сразу же, когда eventInvokeFlag = true.
    //onlyEventInvoked - Если true, делает вызов clbk только по eventInvokeFlag, при этом между вызовами должно пройти delay или больше времени.
    //  Если false, вызывает постоянно дополнительно по таймеру.
    TimeWorker(unsigned short delay, 
      void (*clbk)(bool), 
      bool *eventInvokeFlag = NULL, 
      bool onlyEventInvoked = true, 
      bool useMicros = false)
    {
      _delay = delay;
      _clbk = clbk;
      _useMicros = useMicros;
      _lastExecTime = _useMicros ? micros() : millis();
      _eventInvokeFlag = eventInvokeFlag;
      _onlyEventInvoked = onlyEventInvoked;
    }

    void Update()
    {
      unsigned long currentTime = _useMicros ? micros() : millis();;
      bool invoke = (currentTime - _lastExecTime) > _delay;
      bool clbkArg = false;

      if (_eventInvokeFlag != NULL)
      {
        invoke = (invoke && _onlyEventInvoked && (*_eventInvokeFlag)) || (!_onlyEventInvoked && (invoke || (*_eventInvokeFlag)));
        clbkArg = (*_eventInvokeFlag);
        *_eventInvokeFlag = false;
      }

      if (invoke)
      {
        (*_clbk)(clbkArg);
        _lastExecTime = currentTime;
      }
    }

    void SetOnlyEventInvoked(bool value)
    {
      _onlyEventInvoked = value;
    }
};
//-------------------- end time worker


//-------------------- stack
template<typename T> class Node
{
  public:
    T Value;
    Node* NextNode;
    Node(const T value, Node* nextNode)
    {
      Value = value;
      NextNode = nextNode;
    }
};

template<typename T> class NodeIterator
{
  private:
    Node<T>* _currentNode;

  public:
    NodeIterator(Node<T> *startNode)
    {
      _currentNode = startNode;
    }

    bool MoveNext()
    {
      if (_currentNode->NextNode == NULL)
        return false;

      _currentNode = _currentNode->NextNode;
      return true;
    }

    T GetValue()
    {
      return _currentNode->Value;
    }

    T* GetReferenceValue()
    {
      return &_currentNode->Value;
    }

    void SetValue(T value)
    {
      _currentNode->Value = value;
    }
};

template<typename T> class Stack
{
  private:
    Node<T>* _currentNode;
    unsigned int _count;
    T _default;

  public:
    Stack()
    {
      _currentNode = NULL;
      _count = 0;
    }

    void Clear()
    {
      while (_count != 0)
      {
        Pop();
      }
    }

    ~Stack()
    {
      Clear();
    }

    void Push (const T item)
    {
      Node<T>* newNode = new Node<T>(item, NULL);
      
      if (_count == 0)
      {
        _currentNode = newNode;
      }
      else
      {
        newNode->NextNode = _currentNode;
        _currentNode = newNode;
      }

      _count++;
    }
    
    T Pop()
    {
      if (_count == 0)
        return _default;

      Node<T>* current = _currentNode;
      T currentVal = current->Value;
      _currentNode = current->NextNode;
      _count--;
      delete current;

      return currentVal;
    }

    bool Contains(T val)
    {
      if (_count == 0)
        return false;

      Node<T>* node = _currentNode;
      while (node != NULL)
      {
        if (memcmp(&node->Value, &val, sizeof(T)) == 0)
          return true;

        node = node->NextNode;
      }

      return false;
    }

    void Delete(T val)
    {
      if (_count == 0)
        return;

      Node<T>* node = _currentNode;

      if (memcmp(&node->Value, &val, sizeof(T)) == 0)
      {
        _currentNode = node->NextNode;

        delete node;
        _count--;
        return;
      }
      
      while (node->NextNode != NULL)
      {
        if (memcmp(&node->NextNode->Value, &val, sizeof(T)) == 0)
        {
          Node<T>* delNode = node->NextNode;
          node->NextNode = delNode->NextNode;

          delete delNode;
          _count--;
          return;
        }

        node = node->NextNode;
      }
    }

    unsigned int Count()
    {
      return _count;
    }

    NodeIterator<T>* CreateIterator()
    {
      return new NodeIterator<T>(_currentNode);
    }
};
//-------------------- end stack


//-------------------- drawing
void SetCube(uint8_t value)
{
  SetArrRank2<uint8_t>((uint8_t*)CubeBuffer, CUBE_DIMENSION, CUBE_DIMENSION, value);
}

//layer = x
//line = y
//cell = z
void SetPoint(int layer, int line, int cell)
{
  CubeBuffer[layer][line] |= 1 << cell; 
}

void UnSetPoint(int layer, int line, int cell)
{
  CubeBuffer[layer][line] &= ~(1 << cell); 
}

void SetPoint(int layer, int line, int cell, bool value)
{
  if (value)
    SetPoint(layer, line, cell);
  else
    UnSetPoint(layer, line, cell);
}

bool CheckPoint(int layer, int line, int cell)
{
  return CubeBuffer[layer][line] & (1 << cell);
}

void SetPlaneX(int layer, int value)
{
  for (int line = 0; line < CUBE_DIMENSION; ++line)
  {
    CubeBuffer[layer][line] |= value;
  }
}

void SetPalneY(int line, int value)
{
  for (int layer = 0; layer < CUBE_DIMENSION; ++layer)
  {
    CubeBuffer[layer][line] |= value;
  }
}

void SetPlaneZ(int cell, int value)
{
  if (value == 0)
    return;

  for (int layer = 0; layer < CUBE_DIMENSION; ++layer)
  {
    int byteLayer = 1 << layer;
    if ((value & byteLayer) != byteLayer)
      continue;
    
    for (int line = 0; line < CUBE_DIMENSION; ++line)
    {
      CubeBuffer[layer][line] |= 1 << cell;
    }
  }
}

void Line(int x1, int y1, int z1, int x2, int y2, int z2)
{
  double l = sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2) + pow(z2 - z1, 2));
  double dX = (x2 - x1) / l;
  double dY = (y2 - y1) / l;
  double dZ = (z2 - z1) / l;

  double x = x1, y = y1, z = z1;

  for (int i = 0; i <= round(l); ++i)
  {
    SetPoint(round(x), round(y), round(z));
    x += dX;
    y += dY;
    z += dZ;
  }
}
//-------------------- end drawing


int CurrentLayerRender = 0;
void CubeRenderWorkerClbk(bool eventExec)
{
  if (CurrentLayerRender == CUBE_DIMENSION)
  {   
    FastSetPin(LATCH_PIN, LOW);
    for (int layer = 0; layer < CUBE_DIMENSION + 1; ++layer)
    {
      FastShiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, 0);
    }
    FastSetPin(LATCH_PIN, HIGH);

    CurrentLayerRender = 0;
  }

  FastSetPin(LATCH_PIN, LOW);
  FastShiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (1 << CurrentLayerRender) SCHEMA_BUG);
  for (int row = CUBE_DIMENSION - 1; row > -1; --row)
  {
    FastShiftOut(DATA_PIN, CLOCK_PIN, MSBFIRST, (CubeBuffer[CurrentLayerRender][row]) SCHEMA_BUG);
  }
  FastSetPin(LATCH_PIN, HIGH);

  CurrentLayerRender++;  
}
TimeWorker CubeRenderWorker = TimeWorker(CUBE_RENDER_LAYER_DELAY, CubeRenderWorkerClbk, NULL, true, true);


const int rainN = 10; 
Point rain[rainN];
void RainEffectWorkerClbk(bool eventExec)
{
  SetCube(0);
  
  for (int i = 0; i < rainN; ++i)
  {
    rain[i].Y++;
    
    if (rain[i].Y == CUBE_DIMENSION)
    {
      rain[i].X = random(CUBE_DIMENSION);
      rain[i].Y = random(CUBE_DIMENSION);       
      rain[i].Y = 0;
    }

    SetPoint(rain[i].X, rain[i].Y, rain[i].Z);       
  }
}
TimeWorker RainEffectWorker = TimeWorker(RAIN_EFFECT_DELAY, RainEffectWorkerClbk);


Point Pongs[PONG_ELEMENT_COUNT][PONG_LAIL_LENGHT];
Point PongDirections[PONG_ELEMENT_COUNT];
int8_t PongTailIndexes[PONG_ELEMENT_COUNT];
void InitPong()
{
  for (int i = 0; i < PONG_ELEMENT_COUNT; ++i)
  {
    PongTailIndexes[i] = PONG_LAIL_LENGHT - 1;
    PongDirections[i] = { .X = 1, .Y = 1, .Z = 1 };
  }
}
void PongWorkerClbk(bool eventExec)
{
  SetCube(0);
  
  for (int i = 0; i < PONG_ELEMENT_COUNT; ++i)
  {
    Point past = Pongs[i][0];
    Point direction = PongDirections[i];

    Pongs[i][0].X += direction.X;
    Pongs[i][0].Y += direction.Y;
    Pongs[i][0].Z += direction.Z;

    if (PONG_LAIL_LENGHT > 1)
    {
      Pongs[i][PongTailIndexes[i]] = past;

      PongTailIndexes[i]--;

      if (PongTailIndexes[i] == 0)
        PongTailIndexes[i] = PONG_LAIL_LENGHT - 1;
    }

    if (Pongs[i][0].X >= CUBE_DIMENSION)
    {
      Pongs[i][0].X = CUBE_DIMENSION - 1;
      PongDirections[i].X = random(100) > PONG_THRESHOLD ? -direction.X : direction.X;
    }
    else if (Pongs[i][0].X < 0)
    {
      Pongs[i][0].X = 0;
      PongDirections[i].X = random(100) > PONG_THRESHOLD ? -direction.X : direction.X;
    }

    if (Pongs[i][0].Y >= CUBE_DIMENSION)
    {
      Pongs[i][0].Y = CUBE_DIMENSION - 1;
      PongDirections[i].Y = random(100) > PONG_THRESHOLD ? -direction.Y : direction.Y;
    }
    else if (Pongs[i][0].Y < 0)
    {
      Pongs[i][0].Y = 0;
      PongDirections[i].Y = random(100) > PONG_THRESHOLD ? -direction.Y : direction.Y;
    }

    if (Pongs[i][0].Z >= CUBE_DIMENSION)
    {
      Pongs[i][0].Z = CUBE_DIMENSION - 1;
      PongDirections[i].Z = random(100) > PONG_THRESHOLD ? -direction.Z : direction.Z;
    }
    else if (Pongs[i][0].Z < 0)
    {
      Pongs[i][0].Z = 0;
      PongDirections[i].Z = random(100) > PONG_THRESHOLD ? -direction.Z : direction.Z;
    }

    for (int j = 0; j < PONG_LAIL_LENGHT; ++j)
    {
      SetPoint(Pongs[i][j].X, Pongs[i][j].Y, Pongs[i][j].Z);
    }
  }
}
TimeWorker PongWorker = TimeWorker(PONG_EFFECT_DELAY, PongWorkerClbk);


int SetPoints = 0;
bool BreathReverse = false;
bool BreathContinueWorkerInvoked = false;
bool BreathContinueWorkerFlag = false;
void BreathContinueWorkerClbk(bool eventExec)
{
  if (eventExec)
    return;

  BreathContinueWorkerInvoked = false;
  BreathReverse = !BreathReverse;  
  SetPoints = 0;
}
TimeWorker BreathContinueWorker = TimeWorker(BREATH_END_DELAY, BreathContinueWorkerClbk, &BreathContinueWorkerFlag, false);
void BreathWorkerClbk(bool eventExec)
{
  if (SetPoints == MaxCubeLenght)
  {
    if (!BreathContinueWorkerInvoked)
    {
      BreathContinueWorkerInvoked = true;
      BreathContinueWorkerFlag = true;
    }

    BreathContinueWorker.Update();
  }

  int index = random(MaxCubeLenght);
  Point p;
  GetCoordinateFromIndex(index, &p.X, &p.Y, &p.Z);

  if (CheckPoint(p.X, p.Y, p.Z) != BreathReverse)
  {
    int i = 1;
    Point p2 = p;
    bool validInc = (index + i) < MaxCubeLenght, validDec = (index - i) > -1; 
    
    if (validInc)
      GetCoordinateFromIndex(index + i, &p.X, &p.Y, &p.Z);
    if (validDec)
      GetCoordinateFromIndex(index - i, &p2.X, &p2.Y, &p2.Z);

    while (validInc || validDec)
    {
      if (validInc)
        if (CheckPoint(p.X, p.Y, p.Z) == BreathReverse)
          break;
      if (validDec)
        if (CheckPoint(p2.X, p2.Y, p2.Z) == BreathReverse)
          break;        

      if (++i > 5) break;

      validInc = (index + i) < MaxCubeLenght;
      validDec = (index - i) > -1; 

      if (validInc)
        GetCoordinateFromIndex(index + i, &p.X, &p.Y, &p.Z);
      if (validDec)
        GetCoordinateFromIndex(index - i, &p2.X, &p2.Y, &p2.Z);    
    }
      
    if (validInc && (CheckPoint(p.X, p.Y, p.Z) == BreathReverse))
    {
      SetPoint(p.X, p.Y, p.Z, !BreathReverse);
      SetPoints++;
    }
    else if (validDec && (CheckPoint(p2.X, p2.Y, p2.Z) == BreathReverse))
    {
      SetPoint(p2.X, p2.Y, p2.Z, !BreathReverse);
      SetPoints++;
    }
  }
  else
  {
    SetPoint(p.X, p.Y, p.Z, !BreathReverse);
    SetPoints++;
  }
}
TimeWorker BreathWorker = TimeWorker(BREATH_DELAY, BreathWorkerClbk);


int l = 0;
int r = 0;
int c = 0;
int state = 0;
bool ss = false;
int f = 0;
int fkey = false;
void TestEffectWorkerClbk(bool eventExec)
{
  //200
  /*SetCube(0);
  SetPlaneX(f, 255);
  SetPalneY(f, 255);
  SetPlaneZ(f, 255);
  f += fkey ? -1 : 1;
  if (f == CUBE_DIMENSION - 1 || f == 0)
    fkey = !fkey;*/


  //1000
  /*SetCube(0);
  if (ss)
  {
    for (int i = 0; i < CUBE_DIMENSION; ++i)
    {
      SetPoint(0, i, 0);
      SetPoint(0, i, CUBE_DIMENSION - 1);
      SetPoint(0, 0, i);
      SetPoint(0, CUBE_DIMENSION - 1, i);

      SetPoint(CUBE_DIMENSION - 1, i, 0);
      SetPoint(CUBE_DIMENSION - 1, i, CUBE_DIMENSION - 1);
      SetPoint(CUBE_DIMENSION - 1, 0, i);
      SetPoint(CUBE_DIMENSION - 1, CUBE_DIMENSION - 1, i);

      SetPoint(i, 0, 0);
      SetPoint(i, CUBE_DIMENSION - 1, 0);
      SetPoint(i, 0, CUBE_DIMENSION - 1);
      SetPoint(i, CUBE_DIMENSION - 1, CUBE_DIMENSION - 1);
    }
  }
  else
  {
    SetCube(255);  
  }
  ss = !ss;*/


  //500
  /*SetCube(0); 
  SetPoint(l, r, c);
  c++;
  if (c >= CUBE_DIMENSION)
  {
    r++;
    c = 0;
  }
  if (r >= CUBE_DIMENSION)
  {
    l++;
    r = 0;
  }
  if (l >= CUBE_DIMENSION)
  {
    l = 0;
    r = 0;
    c = 0;
  }*/


  //1000
  /*SetCube(0);
  if (state == 0)
  {
    for (int i = 0; i < CUBE_DIMENSION; ++i)
    {
      for (int j = 0; j < CUBE_DIMENSION; ++j)
      {
        SetPoint(0, i, j);        
      }
    }      
  }
  else if (state == 1)
  {
    for (int i = 0; i < CUBE_DIMENSION; ++i)
    {
      for (int j = 0; j < CUBE_DIMENSION; ++j)
      {
        SetPoint(i, CUBE_DIMENSION - 1, j);        
      }
    }         
  }
  else if (state == 2)
  {
    for (int i = 0; i < CUBE_DIMENSION; ++i)
    {
      for (int j = 0; j < CUBE_DIMENSION; ++j)
      {
        SetPoint(i, j, 0);        
      }
    } 
  }
  else if (state == 3)
  {
    for (int i = 0; i < CUBE_DIMENSION; ++i)
    {
      for (int j = 0; j < CUBE_DIMENSION; ++j)
      {
        SetPoint(i, 0, j);        
      }
    }         
  }
  else if (state == 4)
  {
    for (int i = 0; i < CUBE_DIMENSION; ++i)
    {
      for (int j = 0; j < CUBE_DIMENSION; ++j)
      {
        SetPoint(CUBE_DIMENSION - 1, i, j);        
      }
    } 
  }
  else if (state == 5)     
  {
    for (int i = 0; i < CUBE_DIMENSION; ++i)
    {
      for (int j = 0; j < CUBE_DIMENSION; ++j)
      {
        SetPoint(i, j, CUBE_DIMENSION - 1);        
      }
    } 
  }

  state++;   
  if (state == 6)
    state = 0;*/
}
TimeWorker TestEffectWorker = TimeWorker(70, TestEffectWorkerClbk);


void CubeControllerWorkerClbk(bool eventExec)
{
  switch (CurrentAppState)
  {
  case FullMatrixOff:
    SetCube(0);
    break;
  case FullMatrixOn:
    SetCube(255);
    break;
  case RainEffect:
    RainEffectWorker.Update();
    break;
  case PongEffect:
    PongWorker.Update();
    break;
  case BreathEffect:
    BreathWorker.Update();
    break;
  case TestEffect:
    TestEffectWorker.Update();
    break;
  default:
    SetCube(0);
    break;
  }
}
TimeWorker CubeControllerWorker = TimeWorker(CUBE_CONTROLLER_DELAY, CubeControllerWorkerClbk);


void setup() 
{
  Serial.begin(9600);

  randomSeed(analogRead(2));

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  
  CurrentAppState = BreathEffect;

  InitPong();

  for (int i = 0; i < rainN; ++i)
  {
    rain[i].X = random(CUBE_DIMENSION);
    rain[i].Y = random(CUBE_DIMENSION);
    rain[i].Z = random(CUBE_DIMENSION);
  }
}

void loop() 
{
  CubeControllerWorker.Update();
  CubeRenderWorker.Update();
  
  /*for (int i = 0; i < CUBE_DIMENSION; ++i)
  {
    for (int j = 0; j < CUBE_DIMENSION; ++j)
    {
      Serial.print(CubeBuffer[i][j]);
      Serial.print(" ");
    }

    Serial.println();
  }

  Serial.println();
  //delay(1000);*/
}