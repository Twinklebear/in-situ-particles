  float ghosting;

  struct Split {
    box3f domain;
    box3f ghostDomain;
    size_t totalSize;
    std::vector<vec3f> particle;
  };

  Split *initialSplit(const std::vector<vec3f> &particle)
  {
    Split *s = new Split;
    s->domain = computeBounds(particle);
    s->ghostDomain.min = s->domain.min - vec3f(
  }

  void distributeParticles(std::vector<vec3f> &particle)
  {
    std::priority_queue<Split> splitQueue;

    splitQueue.push(initialSplit(particle));
    while (splitQueue.size() < size) {
      Split thisSplit = splitQueue.top();
      splitQueue.pop();

      Split newSplit[2];
      SplitOne(thisSplit,newSplit);
      splitQueue.push(newSplit[0]);
      splitQueue.push(newSplit[1]);
    }

    Split finalSplit[size];
    std::copy(splitQueue.begin(),splitQueue.end(),finalSplit);
    
    std::vector<vec3f> oldParticle = particle;
    for (int src=0;src<size;src++)
      for (int tgt=0;tgt<size;tgt++) {
        if (rank == src) {
          int numToTgt = finalSplit[tgt].myEnd - finalSplit[tgt].myBegin;
          MPI_CALL(Send(&numToTgt,1,MPI_INT,tgt,0,ownComm));
          MPI_CALL(Send(&oldParticle[finalSplit[tgt].myBegin],numToTgt*3,MPI_FLOAT,
                        tgt,0,ownComm));
        }
        if (rank == tgt) {
          int numFromSrc = 0;
          MPI_CALL(Recv(&numToTgt,1,MPI_INT,tgt,0,ownComm,MPI_STATUS_IGNORE));
          MPI_CALL(Send(&particle[finalSplit[tgt].myBegin],numToTgt*3,MPI_FLOAT,
                        tgt,0,ownComm,MPI_STATUS_IGNORE));

        }
      }
    // now, exchange the respective parts ...
  }


