#define CUBE_DIMENSION 5 //1 to 8
#define DATA_PIN 8 //DS
#define LATCH_PIN 9 //STCP
#define CLOCK_PIN 10 //SHCP
#define BUTTONS_PIN 1
#define RANDOM_PIN 2
#define BUTTON_1_THRESHOLD 200
#define BUTTON_2_THRESHOLD 600
#define SCHEMA_BUG << 1 //a bug in my scheme, remove it if the scheme is correct

#define CUBE_RENDER_LAYER_DELAY 500 //micros
#define CUBE_CONTROLLER_DELAY 10 //millis

#define RAIN_EFFECT_DELAY 70 //millis
#define RAIN_DROP_COUNT 10

#define PONG_EFFECT_DELAY 40 //millis
#define PONG_ELEMENT_COUNT 1
#define PONG_TLAIL_LENGHT 5
#define PONG_THRESHOLD 20 //%

#define BREATH_DELAY 70 //millis
#define BREATH_END_DELAY 1400 //millis

#define FLIP_FLOP_EFFECT_DELAY 80 //millis

#define STARS_EFFECT_DELAY 500 //millis

#define START_LAYER_EFFECT_DELAY 600 //millis
#define LAYER_FORCE_DELAY 300 //millis
#define LAYER_SPEED_FORCE 30
#define MAX_SPEED_LOOP_COUNT 5

#define CUBE_EFFECT_DELAY 400 //millis

#define TEXT_EFFECT_DELAY 150 //millis
#define TEXT_ASCII_START_INDEX 65

#define BORDER_EFFECT_DELAY 50 //millis

#define WAVE_EFFECT_DELAY 70 //millis
#define WAVE_COUNTER_INC 0.5


enum AppState : int
{  
  FullMatrixOn = 0,
  RainEffect = 1,
  PongEffect = 2,
  BreathEffect = 3,
  FlipFlopEffect = 4,
  StarsEffect = 5,
  LayerEffect = 6,
  CubeEffect = 7,
  BorderEffect = 8,
  TextEffect = 9,
  WaveEffect = 10,
  FullMatrixOff = 11
};

enum NXYZ : int
{
  Ne = 0,
  Xe = 1,
  Ye = 2,
  Ze = 3
};

struct Point
{
  int8_t X;
  int8_t Y;
  int8_t Z;
};

struct PointFloat
{
  float X;
  float Y;
  float Z;
};

struct Point2D
{
  int8_t X;
  int8_t Y;
};



const int MaxCubeLenght2 = CUBE_DIMENSION * CUBE_DIMENSION;
const int MaxCubeLenght = CUBE_DIMENSION * CUBE_DIMENSION * CUBE_DIMENSION;
uint8_t CubeBuffer[CUBE_DIMENSION][CUBE_DIMENSION];
AppState CurrentAppState = FullMatrixOff;


//-------------------- common
float RandomFloat(uint8_t accuracy = 1)
{
  int divider = pow(10, accuracy);
  return random(divider) / (float)divider;
}

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

    void SetDelay(unsigned short value)
    {
      _delay = value;
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

void SetPlaneY(int line, int value)
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

void UnSetPlaneX(int layer)
{
  for (int line = 0; line < CUBE_DIMENSION; ++line)
  {
    CubeBuffer[layer][line] = 0;
  }
}

void UnSetPlaneY(int line)
{
  for (int layer = 0; layer < CUBE_DIMENSION; ++layer)
  {
    CubeBuffer[layer][line] = 0;
  }
}

void UnSetPlaneZ(int cell)
{
  for (int layer = 0; layer < CUBE_DIMENSION; ++layer)
  {    
    for (int line = 0; line < CUBE_DIMENSION; ++line)
    {
      CubeBuffer[layer][line] &= ~(1 << cell);
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


PointFloat Rain[RAIN_DROP_COUNT];
float RainSpeed[RAIN_DROP_COUNT];

void SetRainRandomSpeed(int i)
{
  float newSpeed = RandomFloat(3);
  RainSpeed[i] = newSpeed < 0.4 ? 1.0 - newSpeed : newSpeed;
}

void InitRain()
{
  for (int i = 0; i < RAIN_DROP_COUNT; ++i)
  {
    Rain[i].X = random(CUBE_DIMENSION);
    Rain[i].Z = random(CUBE_DIMENSION);       
    Rain[i].Y = random(CUBE_DIMENSION);

    SetRainRandomSpeed(i);
  }
}

void RainEffectWorkerClbk(bool eventExec)
{
  SetCube(0);
  
  for (int i = 0; i < RAIN_DROP_COUNT; ++i)
  {
    Rain[i].Y += RainSpeed[i];
    
    if ((int8_t)Rain[i].Y == CUBE_DIMENSION)
    {
      Rain[i].X = random(CUBE_DIMENSION);
      Rain[i].Z = random(CUBE_DIMENSION);       
      Rain[i].Y = 0;

      SetRainRandomSpeed(i);
    }

    SetPoint(Rain[i].X, Rain[i].Y, Rain[i].Z);       
  }
}

TimeWorker RainEffectWorker = TimeWorker(RAIN_EFFECT_DELAY, RainEffectWorkerClbk);


Point Pongs[PONG_ELEMENT_COUNT][PONG_TLAIL_LENGHT];
Point PongDirections[PONG_ELEMENT_COUNT];
int8_t PongTailIndexes[PONG_ELEMENT_COUNT];

void InitPong()
{
  for (int i = 0; i < PONG_ELEMENT_COUNT; ++i)
  {
    for (int j = 0; j < PONG_TLAIL_LENGHT; ++j)
    {
      Pongs[i][j] = { .X = 0, .Y = 0, .Z = 0 };
    }

    PongTailIndexes[i] = PONG_TLAIL_LENGHT - 1;
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

    if (PONG_TLAIL_LENGHT > 1)
    {
      Pongs[i][PongTailIndexes[i]] = past;

      PongTailIndexes[i]--;

      if (PongTailIndexes[i] == 0)
        PongTailIndexes[i] = PONG_TLAIL_LENGHT - 1;
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

    for (int j = 0; j < PONG_TLAIL_LENGHT; ++j)
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

void InitBreath()
{
  SetCube(0);
  SetPoints = 0;
  BreathReverse = false;
  BreathContinueWorkerInvoked = false;
  BreathContinueWorkerFlag = false;
}

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


Point ParentLine[2];
Point ChildLine[2];
Point DynamicChildLine[2];
NXYZ FirstStap;
int FirstStapValue;
NXYZ SecondStap;
int SecondStapValue;

void InitFlipFlop()
{
  ParentLine[0] = { .X = 0, .Y = 0, .Z = 0 };
  ParentLine[1] = { .X = 0, .Y = 0, .Z = CUBE_DIMENSION - 1 };

  ChildLine[0] = { .X = 0, .Y = CUBE_DIMENSION - 1, .Z = 0 };
  ChildLine[1] = { .X = 0, .Y = CUBE_DIMENSION - 1, .Z = CUBE_DIMENSION - 1 };

  DynamicChildLine[0] = ChildLine[0];
  DynamicChildLine[1] = ChildLine[1];

  FirstStap = Xe;
  FirstStapValue = 1;
  SecondStap = Ye;
  SecondStapValue = -1;
}

int SearchIndexChildPoint(Point *parentP)
{
  int resEq = 0;

  if (ChildLine[0].X == parentP->X)
    resEq++;
  if (ChildLine[0].Y == parentP->Y)
    resEq++;
  if (ChildLine[0].Z == parentP->Z)
    resEq++;

  return resEq >= 2 ? 0 : 1;
}

NXYZ GetPlaneDifference(Point *p1, Point *p2)
{
  if (p1->X != p2->X)
    return Xe;  
  if (p1->Y != p2->Y)
    return Ye;  
  if (p1->Z != p2->Z)
    return Ze;

  return Ne;
}

int GetPlaneDifferenceValue(Point *p1, Point *p2, int planeDiff)
{
  switch (planeDiff)
  {
    case Xe:
      return (p2->X - p1->X) > 0 ? 1 : -1;  
    case Ye:
      return (p2->Y - p1->Y) > 0 ? 1 : -1;
    case Ze:
      return (p2->Z - p1->Z) > 0 ? 1 : -1;
    default:
      return 0;
  }
}

void InitStaps()
{
  int rndIndex = random(2);
  Point parent1 = ParentLine[rndIndex];
  Point parent2 = DynamicChildLine[SearchIndexChildPoint(&parent1)];
  Point child1 = ParentLine[rndIndex == 0 ? 1 : 0];
  Point child2 = DynamicChildLine[SearchIndexChildPoint(&child1)];

  ParentLine[0] = parent1;
  ParentLine[1] = parent2;
  ChildLine[0] = child1;
  ChildLine[1] = child2;
  DynamicChildLine[0] = child1;
  DynamicChildLine[1] = child2; 

  NXYZ parentDiff = GetPlaneDifference(&ParentLine[0], &ParentLine[1]);
  
  switch (parentDiff)
  {
    case Xe:
      if (parent1.Y == child1.Y)
      {
        FirstStap = Ye;
        SecondStap = Ze;

        FirstStapValue = parent1.Y == 0 ? 1 : -1;
        SecondStapValue = child1.Z == 0 ? 1 : -1;
      }
      else
      {
        FirstStap = Ze;
        SecondStap = Ye;

        FirstStapValue = parent1.Z == 0 ? 1 : -1;
        SecondStapValue = child1.Y == 0 ? 1 : -1;
      }
      break;
    case Ye:
      if (parent1.X == child1.X)
      {
        FirstStap = Xe;
        SecondStap = Ze;

        FirstStapValue = parent1.X == 0 ? 1 : -1;
        SecondStapValue = child1.Z == 0 ? 1 : -1;
      }
      else
      {
        FirstStap = Ze;
        SecondStap = Xe;

        FirstStapValue = parent1.Z == 0 ? 1 : -1;
        SecondStapValue = child1.X == 0 ? 1 : -1;
      }
      break;
    case Ze:
      if (parent1.X == child1.X)
      {
        FirstStap = Xe;
        SecondStap = Ye;

        FirstStapValue = parent1.X == 0 ? 1 : -1;
        SecondStapValue = child1.Y == 0 ? 1 : -1;
      }
      else
      {
        FirstStap = Ye;
        SecondStap = Xe;

        FirstStapValue = parent1.Y == 0 ? 1 : -1;
        SecondStapValue = child1.X == 0 ? 1 : -1;
      }
      break;  
  }  
}

void DrawLines()
{
  Point start = ParentLine[0];
  Point end = DynamicChildLine[SearchIndexChildPoint(&start)];
  NXYZ planeDiff = GetPlaneDifference(&ParentLine[0], &ParentLine[1]);
  int planeDiffValue = GetPlaneDifferenceValue(&ParentLine[0], &ParentLine[1], planeDiff);

  for (int i = 0; i < CUBE_DIMENSION; ++i)
  {
    Line(start.X, start.Y, start.Z, end.X, end.Y, end.Z);

    switch (planeDiff)
    {
      case Xe:
        start.X += planeDiffValue;
        end.X += planeDiffValue;
        break;    
      case Ye:
        start.Y += planeDiffValue;
        end.Y += planeDiffValue;
        break;
      case Ze:
        start.Z += planeDiffValue;
        end.Z += planeDiffValue;
        break;
    }
  }
}

void FlipFlopClbk(bool eventExec)
{
  SetCube(0);

  DrawLines();

  int8_t *firstStapField0;
  int8_t *firstStapField1;
  switch (FirstStap)
  {
    case Xe:
      firstStapField0 = &DynamicChildLine[0].X;
      firstStapField1 = &DynamicChildLine[1].X;
      break;
    case Ye:
      firstStapField0 = &DynamicChildLine[0].Y;
      firstStapField1 = &DynamicChildLine[1].Y;
      break;
    case Ze:
      firstStapField0 = &DynamicChildLine[0].Z;
      firstStapField1 = &DynamicChildLine[1].Z;
      break;  
  }

  bool isNextStap = FirstStapValue > 0 ? (*firstStapField0) == CUBE_DIMENSION - 1 : (*firstStapField0) == 0;

  if (isNextStap)
  {
    int8_t *secondStapField0;
    int8_t *secondStapField1;
    switch (SecondStap)
    {
      case Xe:
        secondStapField0 = &DynamicChildLine[0].X;
        secondStapField1 = &DynamicChildLine[1].X;
        break;
      case Ye:
        secondStapField0 = &DynamicChildLine[0].Y;
        secondStapField1 = &DynamicChildLine[1].Y;
        break;
      case Ze:
        secondStapField0 = &DynamicChildLine[0].Z;
        secondStapField1 = &DynamicChildLine[1].Z;
        break;  
    }

    isNextStap = SecondStapValue > 0 ? (*secondStapField0) == CUBE_DIMENSION - 1 : (*secondStapField0) == 0;

    if (isNextStap)
    {
      InitStaps();
    }
    else
    {
      *secondStapField0 += SecondStapValue;
      *secondStapField1 += SecondStapValue;
    }    
  }
  else
  {
    *firstStapField0 += FirstStapValue;
    *firstStapField1 += FirstStapValue;
  }  
}

TimeWorker FlipFlopWorker = TimeWorker(FLIP_FLOP_EFFECT_DELAY, FlipFlopClbk);


void InitStars()
{
  SetCube(0);
}

void StarsWorkerClbk(bool eventExec)
{
  if (random(100) > 50)
    SetPoint(random(CUBE_DIMENSION), random(CUBE_DIMENSION), random(CUBE_DIMENSION));
  else
    UnSetPoint(random(CUBE_DIMENSION), random(CUBE_DIMENSION), random(CUBE_DIMENSION));
}

TimeWorker StarsWorker = TimeWorker(STARS_EFFECT_DELAY, StarsWorkerClbk);


NXYZ Layer = Ye;
int LayerIndex = 0;
int LayerSpeed = START_LAYER_EFFECT_DELAY;
int LayerSpeedForce = -LAYER_SPEED_FORCE;
int MaxSpeedLoopCounter = 0;

void InitLayer()
{
  Layer = Ye;
  LayerIndex = 0;
  LayerSpeed = START_LAYER_EFFECT_DELAY;
  LayerSpeedForce = -LAYER_SPEED_FORCE;
  MaxSpeedLoopCounter = 0;
}

void LayerWorkerClbk(bool eventExec)
{
  SetCube(0);

  switch (Layer)
  {
    case Xe:
      SetPlaneX(LayerIndex, 255);
      break;
    case Ye:
      SetPlaneY(LayerIndex, 255);
      break;
    case Ze:
      SetPlaneZ(LayerIndex, 255);
      break;
  }

  LayerIndex++;
  LayerIndex %= CUBE_DIMENSION;
}

TimeWorker LayerWorker = TimeWorker(START_LAYER_EFFECT_DELAY, LayerWorkerClbk);

void LayerForceWorkerClbk(bool eventExec)
{
  LayerSpeed += LayerSpeedForce;

  if (LayerSpeed <= abs(LayerSpeedForce))
  {
    LayerSpeed = abs(LayerSpeedForce);

    if (++MaxSpeedLoopCounter == MAX_SPEED_LOOP_COUNT)
    {
      MaxSpeedLoopCounter = 0;
      LayerSpeedForce = -LayerSpeedForce;
    }
  }
  else if (LayerSpeed >= START_LAYER_EFFECT_DELAY)
  {
    LayerSpeed = START_LAYER_EFFECT_DELAY;
    LayerSpeedForce = -LayerSpeedForce;

    switch (Layer)
    {
      case Xe:
        Layer = Ye;
        break;
      case Ye:
        Layer = Ze;
        break;
      case Ze:
        Layer = Xe;
        break;
    }

    LayerIndex = 0;
  }

  LayerWorker.SetDelay(LayerSpeed);
}

TimeWorker LayerForceWorker = TimeWorker(LAYER_FORCE_DELAY, LayerForceWorkerClbk);


NXYZ BorderDiraction = Ze;
Point BorderPoint;
int8_t BorderInc = 1;
bool BorderBlackOut = false;

void InitBorder()
{
  SetCube(0);

  BorderDiraction = Ze;
  BorderPoint = { .X = 0, .Y = 0, .Z = 0 };
  BorderInc = 1;
  BorderBlackOut = false;
}

void BorderWorkerClbk(bool eventExec)
{
  if (BorderBlackOut)  
    UnSetPoint(BorderPoint.X, BorderPoint.Y, BorderPoint.Z);  
  else  
    SetPoint(BorderPoint.X, BorderPoint.Y, BorderPoint.Z);

  bool reInitDiraction = false;

  switch (BorderDiraction)
  {
    case Xe:
      BorderPoint.X += BorderInc;

      if (BorderInc > 0 && BorderPoint.X == CUBE_DIMENSION)
      {
        BorderPoint.X = CUBE_DIMENSION - 1;
        reInitDiraction = true;
      }
      else if (BorderInc < 0 && BorderPoint.X < 0)
      {
        BorderPoint.X = 0;
        reInitDiraction = true;
      }
      break;
    case Ye:
      BorderPoint.Y += BorderInc;

      if (BorderInc > 0 && BorderPoint.Y == CUBE_DIMENSION)
      {
        BorderPoint.Y = CUBE_DIMENSION - 1;
        reInitDiraction = true;
      }
      else if (BorderInc < 0 && BorderPoint.Y < 0)
      {
        BorderPoint.Y = 0;
        reInitDiraction = true;
      }
      break;
    case Ze:
      BorderPoint.Z += BorderInc;

      if (BorderInc > 0 && BorderPoint.Z == CUBE_DIMENSION)
      {
        BorderPoint.Z = CUBE_DIMENSION - 1;
        reInitDiraction = true;
      }
      else if (BorderInc < 0 && BorderPoint.Z < 0)
      {
        BorderPoint.Z = 0;
        reInitDiraction = true;
      }
      break;
  }

  if (reInitDiraction)
  {   
    int diractionRandomLength = 0;
    NXYZ diractionRandom[3];

    if (BorderDiraction != Xe)
      diractionRandom[diractionRandomLength++] = Xe;
    if (BorderDiraction != Ye)
      diractionRandom[diractionRandomLength++] = Ye;
    if (BorderDiraction != Ze)
      diractionRandom[diractionRandomLength++] = Ze;

    BorderDiraction = diractionRandom[random(diractionRandomLength)];

    switch (BorderDiraction)
    {
      case Xe:
        BorderInc = BorderPoint.X == 0 ? 1 : -1;
        BorderBlackOut = CheckPoint(BorderPoint.X + BorderInc, BorderPoint.Y, BorderPoint.Z);
        break;
      case Ye:
        BorderInc = BorderPoint.Y == 0 ? 1 : -1;
        BorderBlackOut = CheckPoint(BorderPoint.X, BorderPoint.Y + BorderInc, BorderPoint.Z);
        break;
      case Ze:
        BorderInc = BorderPoint.Z == 0 ? 1 : -1;
        BorderBlackOut = CheckPoint(BorderPoint.X, BorderPoint.Y, BorderPoint.Z + BorderInc);        
        break;
    }
  }
}

TimeWorker BorderWorker = TimeWorker(BORDER_EFFECT_DELAY, BorderWorkerClbk);


int CubeRadius = 0;
int CubeRadiusCounter = 1;

void InitCubeEffect()
{
  CubeRadius = 0;
  CubeRadiusCounter = 1;  
}

void CubeEffectWorkerClbk(bool eventExec)
{
  SetCube(255);

  for (int i = 0; i < CubeRadius; ++i)
  {
    UnSetPlaneX(i);
    UnSetPlaneX(CUBE_DIMENSION - 1 - i);
    UnSetPlaneY(i);
    UnSetPlaneY(CUBE_DIMENSION - 1 - i);
    UnSetPlaneZ(i);
    UnSetPlaneZ(CUBE_DIMENSION - 1 - i);
  }  

  CubeRadius += CubeRadiusCounter;

  if (CubeRadius == round(CUBE_DIMENSION / 2.0))
  {
    CubeRadiusCounter = -CubeRadiusCounter;
    CubeRadius--;
  }
  else if (CubeRadius == -1)
  {
    CubeRadiusCounter = -CubeRadiusCounter;
    CubeRadius++;
  }
}
TimeWorker CubeEffectWorker = TimeWorker(CUBE_EFFECT_DELAY, CubeEffectWorkerClbk);


const Point2D CharA[10] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 95, .Y = 95 },
	{ .X = 95, .Y = 95 },
	{ .X = 64, .Y = 0 },
	{ .X = 0, .Y = 127 },
	{ .X = 32, .Y = 95 },
	{ .X = 32, .Y = 95 },
	{ .X = 64, .Y = 0 },
	{ .X = 95, .Y = 95 },
	{ .X = 32, .Y = 95 },
};

const Point2D CharB[18] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 127, .Y = 0 },
	{ .X = 32, .Y = 0 },
	{ .X = 32, .Y = 0 },
	{ .X = 0, .Y = 32 },
	{ .X = 0, .Y = 32 },
	{ .X = 32, .Y = 64 },
	{ .X = 32, .Y = 64 },
	{ .X = 127, .Y = 64 },
	{ .X = 32, .Y = 64 },
	{ .X = 0, .Y = 95 },
	{ .X = 0, .Y = 95 },
	{ .X = 32, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 127, .Y = 127 },
	{ .X = 95, .Y = 127 },
	{ .X = 95, .Y = 0 },
};

const Point2D CharC[14] PROGMEM = 
{
	{ .X = 0, .Y = 127 },
	{ .X = 95, .Y = 127 },
	{ .X = 95, .Y = 127 },
	{ .X = 127, .Y = 95 },
	{ .X = 127, .Y = 95 },
	{ .X = 127, .Y = 32 },
	{ .X = 127, .Y = 32 },
	{ .X = 95, .Y = 0 },
	{ .X = 95, .Y = 0 },
	{ .X = 0, .Y = 0 },
	{ .X = 95, .Y = 32 },
	{ .X = 95, .Y = 32 },
	{ .X = 95, .Y = 95 },
	{ .X = 95, .Y = 95 },
};

const Point2D CharD[14] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 95, .Y = 0 },
	{ .X = 95, .Y = 127 },
	{ .X = 95, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 0, .Y = 95 },
	{ .X = 0, .Y = 95 },
	{ .X = 0, .Y = 32 },
	{ .X = 0, .Y = 32 },
	{ .X = 32, .Y = 0 },
	{ .X = 32, .Y = 0 },
	{ .X = 127, .Y = 0 },
};

const Point2D CharE[10] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 95, .Y = 0 },
	{ .X = 95, .Y = 127 },
	{ .X = 127, .Y = 127 },
	{ .X = 0, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 0, .Y = 0 },
	{ .X = 127, .Y = 64 },
	{ .X = 0, .Y = 64 },
};

const Point2D CharF[6] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 127, .Y = 0 },
	{ .X = 0, .Y = 0 },
	{ .X = 127, .Y = 64 },
	{ .X = 32, .Y = 64 },
};

const Point2D CharG[10] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 127, .Y = 0 },
	{ .X = 32, .Y = 0 },
	{ .X = 127, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 0, .Y = 95 },
	{ .X = 0, .Y = 95 },
	{ .X = 32, .Y = 64 },
	{ .X = 64, .Y = 64 },
};

const Point2D CharH[6] PROGMEM = 
{
	{ .X = 0, .Y = 0 },
	{ .X = 0, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 64 },
	{ .X = 0, .Y = 64 },
};

const Point2D CharI[8] PROGMEM = 
{
	{ .X = 64, .Y = 0 },
	{ .X = 64, .Y = 127 },
	{ .X = 64, .Y = 0 },
	{ .X = 64, .Y = 127 },
	{ .X = 95, .Y = 0 },
	{ .X = 32, .Y = 0 },
	{ .X = 95, .Y = 127 },
	{ .X = 32, .Y = 127 },
};

const Point2D CharJ[6] PROGMEM = 
{
	{ .X = 32, .Y = 0 },
	{ .X = 32, .Y = 127 },
	{ .X = 64, .Y = 127 },
	{ .X = 95, .Y = 95 },
	{ .X = 32, .Y = 0 },
	{ .X = 64, .Y = 0 },
};

const Point2D CharK[6] PROGMEM = 
{
	{ .X = 127, .Y = 0 },
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 64 },
	{ .X = 0, .Y = 0 },
	{ .X = 127, .Y = 64 },
	{ .X = 0, .Y = 127 },
};

const Point2D CharL[6] PROGMEM = 
{
	{ .X = 127, .Y = 0 },
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 32, .Y = 95 },
};

const Point2D CharM[8] PROGMEM = 
{
	{ .X = 64, .Y = 64 },
	{ .X = 127, .Y = 0 },
	{ .X = 64, .Y = 64 },
	{ .X = 0, .Y = 0 },
	{ .X = 0, .Y = 127 },
	{ .X = 0, .Y = 0 },
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 0 },
};

const Point2D CharN[6] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 0, .Y = 127 },
	{ .X = 0, .Y = 0 },
	{ .X = 0, .Y = 127 },
	{ .X = 127, .Y = 0 },
};

const Point2D CharO[8] PROGMEM = 
{
	{ .X = 127, .Y = 32 },
	{ .X = 127, .Y = 95 },
	{ .X = 95, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 0, .Y = 95 },
	{ .X = 0, .Y = 32 },
	{ .X = 32, .Y = 0 },
	{ .X = 95, .Y = 0 },
};

const Point2D CharP[8] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 32 },
	{ .X = 95, .Y = 0 },
	{ .X = 32, .Y = 0 },
	{ .X = 0, .Y = 32 },
	{ .X = 0, .Y = 64 },
	{ .X = 32, .Y = 95 },
	{ .X = 127, .Y = 95 },
};

const Point2D CharQ[10] PROGMEM = 
{
	{ .X = 127, .Y = 32 },
	{ .X = 127, .Y = 95 },
	{ .X = 95, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 32, .Y = 95 },
	{ .X = 32, .Y = 32 },
	{ .X = 32, .Y = 0 },
	{ .X = 95, .Y = 0 },
	{ .X = 64, .Y = 95 },
	{ .X = 0, .Y = 127 },
};

const Point2D CharR[10] PROGMEM = 
{
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 0 },
	{ .X = 127, .Y = 0 },
	{ .X = 32, .Y = 0 },
	{ .X = 32, .Y = 0 },
	{ .X = 32, .Y = 64 },
	{ .X = 32, .Y = 64 },
	{ .X = 127, .Y = 64 },
	{ .X = 95, .Y = 64 },
	{ .X = 32, .Y = 127 },
};

const Point2D CharS[14] PROGMEM = 
{
	{ .X = 0, .Y = 0 },
	{ .X = 95, .Y = 0 },
	{ .X = 95, .Y = 0 },
	{ .X = 127, .Y = 32 },
	{ .X = 127, .Y = 32 },
	{ .X = 95, .Y = 64 },
	{ .X = 95, .Y = 64 },
	{ .X = 32, .Y = 64 },
	{ .X = 32, .Y = 64 },
	{ .X = 0, .Y = 95 },
	{ .X = 0, .Y = 95 },
	{ .X = 32, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 127, .Y = 127 },
};

const Point2D CharT[6] PROGMEM = 
{
	{ .X = 64, .Y = 127 },
	{ .X = 64, .Y = 0 },
	{ .X = 64, .Y = 127 },
	{ .X = 64, .Y = 0 },
	{ .X = 127, .Y = 0 },
	{ .X = 0, .Y = 0 },
};

const Point2D CharU[6] PROGMEM = 
{
	{ .X = 127, .Y = 0 },
	{ .X = 127, .Y = 95 },
	{ .X = 95, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 0, .Y = 95 },
	{ .X = 0, .Y = 0 },
};

const Point2D CharV[4] PROGMEM = 
{
	{ .X = 127, .Y = 0 },
	{ .X = 64, .Y = 127 },
	{ .X = 0, .Y = 0 },
	{ .X = 64, .Y = 127 },
};

const Point2D CharW[8] PROGMEM = 
{
	{ .X = 127, .Y = 0 },
	{ .X = 95, .Y = 127 },
	{ .X = 95, .Y = 127 },
	{ .X = 64, .Y = 64 },
	{ .X = 0, .Y = 0 },
	{ .X = 32, .Y = 127 },
	{ .X = 32, .Y = 127 },
	{ .X = 64, .Y = 64 },
};

const Point2D CharX[4] PROGMEM = 
{
	{ .X = 127, .Y = 0 },
	{ .X = 0, .Y = 127 },
	{ .X = 0, .Y = 0 },
	{ .X = 127, .Y = 127 },
};

const Point2D CharY[8] PROGMEM = 
{
	{ .X = 64, .Y = 127 },
	{ .X = 64, .Y = 64 },
	{ .X = 64, .Y = 127 },
	{ .X = 64, .Y = 64 },
	{ .X = 64, .Y = 64 },
	{ .X = 0, .Y = 0 },
	{ .X = 64, .Y = 64 },
	{ .X = 127, .Y = 0 },
};

const Point2D CharZ[6] PROGMEM = 
{
	{ .X = 127, .Y = 0 },
	{ .X = 0, .Y = 0 },
	{ .X = 0, .Y = 0 },
	{ .X = 127, .Y = 127 },
	{ .X = 127, .Y = 127 },
	{ .X = 0, .Y = 127 },
};

const Point2D *Chars[26] = 
{
  CharA,
  CharB,
  CharC,
  CharD,
  CharE,
  CharF,
  CharG,
  CharH,
  CharI,
  CharJ,
  CharK,
  CharL,
  CharM,
  CharN,
  CharO,
  CharP,
  CharQ,
  CharR,
  CharS,
  CharT,
  CharU,
  CharV,
  CharW,
  CharX,
  CharY,
  CharZ,
};

const int8_t CharsSize[26] = 
{
  sizeof(CharA) / sizeof(Point2D),
  sizeof(CharB) / sizeof(Point2D),
  sizeof(CharC) / sizeof(Point2D),
  sizeof(CharD) / sizeof(Point2D),
  sizeof(CharE) / sizeof(Point2D),
  sizeof(CharF) / sizeof(Point2D),
  sizeof(CharG) / sizeof(Point2D),
  sizeof(CharH) / sizeof(Point2D),
  sizeof(CharI) / sizeof(Point2D),
  sizeof(CharJ) / sizeof(Point2D),
  sizeof(CharK) / sizeof(Point2D),
  sizeof(CharL) / sizeof(Point2D),
  sizeof(CharM) / sizeof(Point2D),
  sizeof(CharN) / sizeof(Point2D),
  sizeof(CharO) / sizeof(Point2D),
  sizeof(CharP) / sizeof(Point2D),
  sizeof(CharQ) / sizeof(Point2D),
  sizeof(CharR) / sizeof(Point2D),
  sizeof(CharS) / sizeof(Point2D),
  sizeof(CharT) / sizeof(Point2D),
  sizeof(CharU) / sizeof(Point2D),
  sizeof(CharV) / sizeof(Point2D),
  sizeof(CharW) / sizeof(Point2D),
  sizeof(CharX) / sizeof(Point2D),
  sizeof(CharY) / sizeof(Point2D),
  sizeof(CharZ) / sizeof(Point2D),
};

int TextZIndex = 0;
int TextCharIndex = 0;
char *Text = "ABCDEFGHIJKLMNOPQRSTUVWXYZ "; 

void InitTextEffect()
{
  TextZIndex = 0;
  TextCharIndex = 0;
}

int CalculatePointValue(int8_t value)
{
  return round(value * (CUBE_DIMENSION - 1) / 127.0);
}

void DrawChar(Point2D *ch, int size, int z)
{
  Point2D p0;
  Point2D p1;

  for (int i = 0; i < size / 2; ++i)
  {
    int index = i * 2;

    memcpy_P(&p0, &ch[index], sizeof(Point2D));
    memcpy_P(&p1, &ch[index + 1], sizeof(Point2D));

    Line(CalculatePointValue(p0.X), CalculatePointValue(p0.Y), z, CalculatePointValue(p1.X), CalculatePointValue(p1.Y), z);   
  }
}

void TextEffectWorkerClbk(bool eventExec)
{
  SetCube(0);

  int charAscii = Text[TextCharIndex] - TEXT_ASCII_START_INDEX;

  if (Text[TextCharIndex] != ' ')
    DrawChar(Chars[charAscii], CharsSize[charAscii], TextZIndex);

  TextZIndex++;
  if (TextZIndex == CUBE_DIMENSION)
  {
    TextZIndex = 0;
    TextCharIndex++;

    if (Text[TextCharIndex] == '\0')
    {
      TextCharIndex = 0;
    }
  }
}
TimeWorker TextEffectWorker = TimeWorker(TEXT_EFFECT_DELAY, TextEffectWorkerClbk);


float CornerCounter = 0;
float CornerCounterInc = WAVE_COUNTER_INC;
const int WaveCenter = CUBE_DIMENSION / 2;

void InitWave()
{
  CornerCounter = 0;
  CornerCounterInc = WAVE_COUNTER_INC;
}

void WaveEffectWorkerClbk(bool eventExec)
{
  SetCube(0);

  for (int layer = 0; layer < CUBE_DIMENSION; ++layer)
  {
    float value = sin(layer * CornerCounterInc + CornerCounter);
    int max = value > 0 ? WaveCenter : CUBE_DIMENSION - 1 - WaveCenter;
    int calcValue = round(value * max);

    CubeBuffer[layer][WaveCenter + calcValue] = 255;
  }

  CornerCounter += CornerCounterInc;
}

TimeWorker WaveEffectWorker = TimeWorker(WAVE_EFFECT_DELAY, WaveEffectWorkerClbk);


bool ButtonPressed = false;

void ReInitEffect()
{
  switch (CurrentAppState)
  {
    case RainEffect:
      InitRain();
      break;
    case PongEffect:
      InitPong();
      break;
    case BreathEffect:
      InitBreath();
      break;
    case FlipFlopEffect:
      InitFlipFlop();
      break;
    case StarsEffect:
      InitStars();
      break;
    case LayerEffect:
      InitLayer();
      break;
    case CubeEffect:
      InitCubeEffect();
      break;
    case BorderEffect:
      InitBorder();
      break;
    case TextEffect:
      InitTextEffect();
      break;
    case WaveEffect:
      InitWave();
      break;
  }
}

void CubeControllerWorkerClbk(bool eventExec)
{
  int buttonValue = analogRead(BUTTONS_PIN);
  
  if (!ButtonPressed)
  {
    if (buttonValue > BUTTON_2_THRESHOLD)
    {
      ButtonPressed = true;
      CurrentAppState = (AppState)((CurrentAppState + 1) % (FullMatrixOff + 1));
      ReInitEffect(); 
    }
    else if (buttonValue > BUTTON_1_THRESHOLD)
    {      
      ButtonPressed = true;
      int state = CurrentAppState - 1;
      CurrentAppState = state < 0 ? FullMatrixOff : (AppState)state;
      ReInitEffect();
    }
  }
  else if (buttonValue < BUTTON_1_THRESHOLD)
  {
    ButtonPressed = false;
  }

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
    case FlipFlopEffect:
      FlipFlopWorker.Update();
      break;
    case StarsEffect:
      StarsWorker.Update();
      break;
    case LayerEffect:
      LayerWorker.Update();
      LayerForceWorker.Update();
      break;
    case CubeEffect:
      CubeEffectWorker.Update();
      break;
    case BorderEffect:
      BorderWorker.Update();
      break;
    case TextEffect:
      TextEffectWorker.Update();
      break;
    case WaveEffect:
      WaveEffectWorker.Update();
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

  randomSeed(analogRead(RANDOM_PIN));

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);

  CurrentAppState = FullMatrixOn;

  ReInitEffect();
}

void loop() 
{
  CubeControllerWorker.Update();
  CubeRenderWorker.Update();
}