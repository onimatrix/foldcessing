void displayInfo()
{
  fill(0);
  textAlign(LEFT, TOP);
  text("FPS: " + nf(frameRate, 0, 1), 10, 10);
  text("Mouse: " + mouseX + ", " + mouseY, 10, 30);
}
