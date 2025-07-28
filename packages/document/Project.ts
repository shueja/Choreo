import { Expr } from "./math/Expr";
import { PROJECT_SCHEMA_VERSION } from "./version/ProjectSchemaVersion";
import { RobotConfig } from "./RobotConfig";
import { SampleType } from "./sample/SampleType";
import { Variables } from "./math/Variable";

export interface Project {
  name: string;
  type: SampleType;
  version: typeof PROJECT_SCHEMA_VERSION;
  variables: Variables;
  config: RobotConfig<Expr>;
}